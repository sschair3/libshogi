#include <cstdlib>
#include <iostream>
#include <Shogi.h>
#include <Array.h>
#include <string>
#include <CSAConnection.h>
#include <random>

// using namespace game;
// using namespace foundation;

int main(void)
{

    // 初期化
    game::Shogi::initialize();

    // 駒の価値を設定
    game::Position::setValue(game::Evaluation::Value);
    game::Position::handsValue(game::Evaluation::Hands);

    // 局面のインスタンスを作成
    game::Position p;

    // 平手初期局面にする
    p.init();

    // csa形式の棋譜データ
    std::string kifu = "'CSA encoding=UTF-8\nV3.0\nN+libshogi_sente\nN-libshogi_gote\nPI\n+\n";

    // 対戦
    while (1)
    {

        // 指し手生成
        foundation::Array<game::Move::Move, game::Move::Max> move;
        p.genMove(move);

        // 手が無かったら詰み
        if (move.vsize() == 0)
        {
            break;
        }

        // ランダムに手を選ぶ
        std::random_device rd;
        std::mt19937 gen(rd());
        std::cout << rd() << std::endl;
        int choice = gen() % (int)move.vsize();

        // 手を表示
        std::cout << p.string(move[choice]) << std::endl;
        kifu += p.string(move[choice]) + "\n";

        // 着手
        p.move(move[choice]);

        // 局面を表示
        std::cout << p << std::endl;
    }

    std::cout << kifu << "\n";

    return 0;
}
