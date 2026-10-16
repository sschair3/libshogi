/**
 * engine.cpp
 *
 * USI プロトコル対応の将棋エンジン本体。
 *
 *   評価部 : このファイルの namespace evaluation (駒の価値の設定と評価関数)
 *   探索部 : Search.h / Search.cpp (search::think)
 *   USI    : ライブラリの game::USIConnection (lib/shogi/Usi.h)
 *
 * ここでは評価部と探索部をつなぎ、USIConnection に「思考 (think)」と
 * エンジンのオプションを実装する。
 *
 * ビルド: make            (Makefile の FILE_NAME が engine)
 * GUI (ShogiHome など) に、できた実行ファイル engine を登録して使う。
 */

#include <cstdlib>
#include <Usi.h>
#include "Search.h"
#include "eval.h"

using namespace game;

/* --------------------------------- 評価部 --------------------------------- */

namespace evaluation
{

    /**
     * 評価部の初期化 (isready のときに 1 回呼ばれる)
     * 駒の価値と持ち駒の価値を設定する。値は lib/shogi/Evaluation.h にある。
     */
    void initialize()
    {
        Position::setValue({//  EMP   BFU   BKY   BKE   BGI   BKA   BHI   BKI
                            0, 90, 315, 405, 495, 855, 990, 540,
                            //  BOU   BTO   BNY   BNK   BNG   BUM   BRY
                            15000, 540, 540, 540, 540, 945, 1395, 0,
                            //        WFU   WKY   WKE   WGI   WKA   WHI   WKI
                            0, -90, -315, -405, -495, -855, -990, -540,
                            //  WOU   WTO   WNY   WNK   WNG   WUM   WRY
                            -15000, -540, -540, -540, -540, -945, -1395, 0});
        Position::handsValue({//  EMP    FU    KY    KE    GI    KA    HI    KI
                              {0, 0, 0, 0, 0, 0, 0, 0},
                              {0, 100, 350, 450, 550, 950, 1100, 600},
                              {0, 200, 700, 900, 1100, 1900, 2200, 1200},
                              {0, 300, 1050, 1350, 1650, 0, 0, 1800},
                              {0, 400, 1400, 1800, 2200, 0, 0, 2400},
                              {0, 500, 0, 0, 0, 0, 0, 0},
                              {0, 600, 0, 0, 0, 0, 0, 0},
                              {0, 700, 0, 0, 0, 0, 0, 0},
                              {0, 800, 0, 0, 0, 0, 0, 0},
                              {0, 900, 0, 0, 0, 0, 0, 0},
                              {0, 1000, 0, 0, 0, 0, 0, 0},
                              {0, 1100, 0, 0, 0, 0, 0, 0},
                              {0, 1200, 0, 0, 0, 0, 0, 0},
                              {0, 1300, 0, 0, 0, 0, 0, 0},
                              {0, 1400, 0, 0, 0, 0, 0, 0},
                              {0, 1500, 0, 0, 0, 0, 0, 0},
                              {0, 1600, 0, 0, 0, 0, 0, 0},
                              {0, 1700, 0, 0, 0, 0, 0, 0},
                              {0, 1800, 0, 0, 0, 0, 0, 0}});
    }

    /**
     * 評価関数: 局面の静的評価値を「先手から見た値」で返す
     * 今は駒割り (盤上の駒と持ち駒の価値の合計) だけ。
     * Position は駒割りを差分計算しているので、eval() で取り出すだけでよい。
     */
    Evaluation::Eval evaluate(Position &p)
    {
        return p.eval();
    }

}

/* ------------------------------ USI エンジン ------------------------------ */

class Engine : public USIConnection
{
public:
    Engine() : USIConnection("my_shogi_lib", "伊藤匠吾") {}

protected:
    /// isready: ライブラリの初期化後に呼ばれる
    void ready() override
    {
        evaluation::initialize();
    }

    /// go: 最善手を探す
    Move::Move think(Position &p, const Usi::Limits &limits) override
    {
        double timeLimit = limits.thinkTime(p.turn());

        search::Result r = search::think(
            p, _maxDepth, timeLimit, evaluation::evaluate,
            [&](const search::Result &it)
            {
                // 1 つの深さを読み切るたびに info を出す
                info(p, it.depth, it.value, it.nodes, it.seconds, it.best);
            });

        // 指せる手がなければ投了
        return r.hasMove ? r.best : Move::None;
    }

    /// usi: GUI に設定項目を知らせる
    void options(std::ostream &os) override
    {
        os << "option name Depth type spin default " << DefaultDepth
           << " min 1 max " << MaxDepth << std::endl;
    }

    /// setoption を受け取ったとき
    void setOption(const std::string &name, const std::string &value) override
    {
        if (name == "Depth")
        {
            int d = std::atoi(value.c_str());
            if (d >= 1 && d <= MaxDepth)
            {
                _maxDepth = d;
            }
        }
    }

private:
    /// 最大探索深さの既定値と上限
    /// (時間で探索を打ち切るので、大きくしておけば持ち時間いっぱいまで読む)
    static const int DefaultDepth = 30;
    static const int MaxDepth = 64;

    /// 最大探索深さ (USI オプション "Depth")
    int _maxDepth = DefaultDepth;
};

int main()
{
    Engine engine;
    engine.run();
    return 0;
}
