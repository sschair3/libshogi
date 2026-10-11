#include <cstdlib>
#include <iostream>
#include <Shogi.h>
#include <Array.h>
#include <CSAConnection.h>

using namespace game;
using namespace foundation;

int main(int argc, char *argv[])
{

    // コマンド実行時の引数確認
    if (argc != 4)
    {
        std::cerr << "Usage : "
                  << argv[0]
                  << " <User Name> <Password> <CSA Server>"
                  << std::endl;
        exit(EXIT_FAILURE);
    }

    // 初期化
    Shogi::initialize();

    // 駒の価値を設定
    Position::setValue(Evaluation::Value);
    Position::handsValue(Evaluation::Hands);

    // CSA サーバに接続
    CSAConnection csa(argv[3]);
    csa.connect();

    // CSA サーバにログイン
    csa.login(argv[1], argv[2]);

    // 対局待ち
    auto summary = csa.newGame();

    // 対局受諾
    csa.accept();

    // 局面のインスタンスを作成
    Position p(summary);

    // プログラムの手番を確認
    auto myturn = Position::myTurn(summary);

    // メッセージ受信用
    Vector<std::string> message;

    // 対戦
    while (1)
    {

        if (p.turn() == myturn)
        {

            // プログラムの手番

            // 指し手生成
            Array<Move::Move, Move::Max> move;
            p.genMove(move);

            // 手が無かったら詰み
            if (move.vsize() == 0)
            {
                // 投了
                csa.resign();
                break;
            }

            // ランダムに手を選ぶ
            int choice = rand() % (int)move.vsize();

            // 指し手送信
            csa.send(p.string(move[choice]));

            // 局面を進める
            p.move(move[choice]);
        }
        else
        {

            // 相手の手番

            // 相手の手を待つ
            csa.receive(message);

            // 送信されてきた手の先頭文字が '+' '-' でないときは指し手ではない
            if (message[0][0] != '+' && message[0][0] != '-')
            {
                // 送信されてきたメッセージを表示
                for (auto l : message)
                {
                    std::cout << l << std::endl;
                }
                break;
            }

            // プログラムの手 (エコー) は無視
            const char turchk[] = {'+', '-'};
            if (message[0][0] == turchk[myturn])
            {
                // 2 行目に相手の手が入る場合もある
                if (message.size() == 2)
                {
                    message[0] = message[1];
                }
                else
                {
                    continue;
                }
            }

            // 相手の手を表示
            std::cout << message[0] << std::endl;

            // 局面を進める
            p.move(message[0]);
        }

        // 局面を表示
        std::cout << p << std::endl;
    }

    // メッセージを全て受信
    while (csa.message())
    {
        csa.receive(message);
        if (message.vsize() == 0)
        {
            break;
        }
        for (auto l : message)
        {
            std::cout << l << std::endl;
        }
    }

    // 棋譜ファイルの書き込み
    csa.write("kifu.csa");

    // ログアウト
    csa.logout();

    // 通信終了
    csa.close();

    exit(EXIT_SUCCESS);
}
