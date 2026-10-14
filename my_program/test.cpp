#include <Shogi.h>
#include <vector>
#include <iostream>
#include <bitset>

int main()
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
    foundation::Array<game::Move::Move, game::Move::Max> move;
    p.genMove(move);
    std::cout << std::bitset<16>(move[0]) << std::endl;
    return 0;
}