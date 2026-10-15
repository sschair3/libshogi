/**
 * usi_engine.cpp
 *
 * USI プロトコル対応の将棋エンジン本体。
 * プロトコル処理はライブラリ (lib/shogi/Usi.h の game::USIConnection) が行い、
 * ここでは探索 (think) とオプションだけを実装する。
 *
 * ビルド: make FILE_NAME=usi_engine
 * GUI (ShogiHome など) にこの実行ファイルをエンジンとして登録して使う。
 */

#include <cstdlib>
#include <Usi.h>
#include "Search.h"
using namespace game;

class MyEngine : public USIConnection
{
public:
    MyEngine() : USIConnection("my_shogi_lib", "伊藤匠吾") {}

protected:
    /// 最善手を探す (go コマンドで呼ばれる)
    Move::Move think(Position &p, const Usi::Limits &limits) override
    {
        double timeLimit = limits.thinkTime(p.turn());
        search::Result r = search::think(p, _maxDepth, timeLimit, [&](const search::Result &it)
                                         { info(p, it.depth, it.value, it.nodes, it.seconds, it.best); });
        // 指せる手がなければ投了
        return r.hasMove ? r.best : Move::None;
    }

    /// GUI に設定項目を知らせる
    void options(std::ostream &os) override
    {
        os << "option name Depth type spin default " << _maxDepth
           << " min 1 max 10" << std::endl;
    }

    /// setoption を受け取ったとき
    void setOption(const std::string &name, const std::string &value) override
    {
        if (name == "Depth")
        {
            int d = std::atoi(value.c_str());
            if (d >= 1)
            {
                _maxDepth = d;
            }
        }
    }

private:
    /// 最大探索深さ
    int _maxDepth = 30;
};

int main()
{
    MyEngine engine;
    engine.run();
    return 0;
}
