#include <iostream>
#include <Shogi.h>

using namespace game;

int main(void)
{

    // ライブラリ初期化
    Shogi::initialize();

    // 駒の価値を設定
    Position::setValue(Evaluation::Value);
    Position::handsValue(Evaluation::Hands);

    // 局面のインスタンスを作成
    Position p;

    // 平手初期局面にする
    p.init();

    // 局面を表示
    std::cout << p << std::endl;

    // 指し手 7六歩を作成
    Move::Move m = Move::move(Square::SQ77, Square::SQ76);

    // 7六 歩を指す
    p.move(m);

    // 再び局面を表示
    std::cout << p << std::endl;

    return 0;
}
