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

#include <algorithm>
#include <cstdlib>
#include <Usi.h>
#include "Search.h"
#include "eval.h"

using namespace game;

/* ------------------------------ USI エンジン ------------------------------ */

class Engine : public USIConnection
{
public:
    Engine() : USIConnection("yukashitaou", "to-i") {}

protected:
    /// isready: ライブラリの初期化後に呼ばれる
    void ready() override
    {
        evaluation::initialize();
    }

    /**
     * 持ち時間から、この手の思考時間 (秒) を決める
     *
     *   目安 = 残り時間 ÷ MovesLeft + (加算 × IncrementRatio または 秒読み − 余裕)
     *     フィッシャーの加算や秒読みは毎手もらえるので、ほぼ全部を使ってよい。
     *     残り時間 (貯金) は、これから指す手数の見込み MovesLeft で割って少しずつ使う
     *   上限 = この手で使える時間 − 通信の遅れなどの余裕 (NetworkDelay)
     *     この手で使える時間 = 残り時間 + 秒読み
     *     フィッシャーの加算は「指した後」に足されるので、この手では使えない
     *   思考時間 = 目安と上限の小さいほう (ただし MinThinkMs 以上)
     *
     * go infinite なら 0 (時間制限なし) を返す
     */
    double decideTime(const Usi::Limits &limits, Color::Color turn) const
    {
        if (limits.infinite)
        {
            return 0.0;
        }
        const double remain = static_cast<double>(turn == Color::Black ? limits.btime : limits.wtime);
        const double inc = static_cast<double>(turn == Color::Black ? limits.binc : limits.winc);
        const double byoyomi = static_cast<double>(limits.byoyomi);
        const double delay = static_cast<double>(_networkDelay);

        double perMove = (inc > 0) ? inc * IncrementRatio : std::max(byoyomi - delay, 0.0);
        double targetMs = remain / MovesLeft + perMove;
        double maxMs = remain + byoyomi - delay;

        return std::max(std::min(targetMs, maxMs), MinThinkMs) / 1000.0;
    }

    /// go: 最善手を探す
    Move::Move think(Position &p, const Usi::Limits &limits) override
    {
        double timeLimit = decideTime(limits, p.turn());

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
        os << "option name NetworkDelay type spin default " << DefaultNetworkDelay
           << " min 0 max 10000" << std::endl;
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
        else if (name == "NetworkDelay")
        {
            int ms = std::atoi(value.c_str());
            if (ms >= 0 && ms <= 10000)
            {
                _networkDelay = ms;
            }
        }
    }

private:
    /// 最大探索深さの既定値と上限
    /// (時間で探索を打ち切るので、大きくしておけば持ち時間いっぱいまで読む)
    static const int DefaultDepth = 30;
    static const int MaxDepth = 64;

    /// 時間の決め方 (decideTime() で使う)
    static const int DefaultNetworkDelay = 1200;  ///< 通信の遅れなどの余裕 (ミリ秒)
    static constexpr double MovesLeft = 25.0;     ///< 残り時間を何手で使い切るつもりか
    static constexpr double IncrementRatio = 0.9; ///< フィッシャーの加算のうち、毎手使う割合
    static constexpr double MinThinkMs = 100.0;   ///< どんなに時間がなくても、これだけは考える

    /// 最大探索深さ (USI オプション "Depth")
    int _maxDepth = DefaultDepth;

    /// 通信の遅れなどの余裕 (USI オプション "NetworkDelay"、ミリ秒)
    int _networkDelay = DefaultNetworkDelay;
};

int main()
{
    Engine engine;
    engine.run();
    return 0;
}
