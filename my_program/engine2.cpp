/**
 * engine2.cpp
 *
 * USI プロトコル対応の将棋エンジン本体。
 *
 *   評価部 : eval.h / eval.cpp (駒割り + 駒の位置の評価)
 *   探索部 : Search.h / Search.cpp (search::think)
 *   USI    : ライブラリの game::USIConnection (lib/shogi/Usi.h)
 *
 * ここでは評価部と探索部をつなぎ、USIConnection に「思考 (think)」と
 * エンジンのオプションを実装する。
 *
 * ビルド: make FILE_NAME=engine2
 * GUI (ShogiHome など) に、できた実行ファイル engine2 を登録して使う。
 */

#include <cstdlib>
#include <Usi.h>
#include "Search.h"
#include "eval.h"

using namespace game;

/* ------------------------------ USI エンジン ------------------------------ */

class Engine : public USIConnection
{
public:
    Engine() : USIConnection("my_shogi_lib 2", "伊藤匠吾") {}

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
    static const int DefaultDepth = 5;
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
