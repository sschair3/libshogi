#include <cstdlib>
#include <iostream>
#include <Shogi.h>
#include <Array.h>
#include <CSAConnection.h>

// using namespace game;
// using namespace foundation;
// using namespace Evaluation;

/* --------------------------- global  variables --------------------------- */

/// 最大の探索深さ
static int _searchDepth = 10;

/* ------------------------------------------------------------------------- */

/* -------------------------- function prototypes -------------------------- */

/// 探索
static size_t searchMax(game::Position &, foundation::Array<game::Move::Move, game::Move::Max> &);
static size_t searchMin(game::Position &, foundation::Array<game::Move::Move, game::Move::Max> &);
static game::Evaluation::Eval quiesMax(game::Position &, game::Evaluation::Eval alpha, game::Evaluation::Eval beta, int depth);
static game::Evaluation::Eval quiesMin(game::Position &, game::Evaluation::Eval alpha, game::Evaluation::Eval beta, int depth);

/* ------------------------------------------------------------------------- */

/**
 * 先手最善手を調べる
 * @parm p 局面
 * @parm m 候補手
 * @return 最善手のインデックス
 */
static size_t searchMax(game::Position &p, foundation::Array<game::Move::Move, game::Move::Max> &m)
{

    size_t idx = 0;
    game::Evaluation::Eval alpha = -game::Evaluation::Infinity;
    game::Evaluation::Eval beta = game::Evaluation::Infinity;

    for (size_t i = 0; i < m.vsize(); ++i)
    {
        // 局面を進める
        auto back = p.move(m[i]);
        // 後手局面を探索
        auto value = quiesMin(p, alpha, beta, 0);
        // 局面を戻す
        p.undo(back);
        // beta カット
        if (value >= beta)
        {
            std::cout << "* " << value << " "
                      << p.string(m[i]) << std::endl;
            return i;
        }
        // 下限を更新
        if (value > alpha)
        {
            alpha = value;
            idx = i;
            std::cout << "  " << value << " "
                      << p.string(m[i]) << std::endl;
        }
    }

    return idx;
}

/**
 * 後手最善手を調べる
 * @parm p 局面
 * @parm m 候補手
 * @return 最善手のインデックス
 */
static size_t searchMin(game::Position &p, foundation::Array<game::Move::Move, game::Move::Max> &m)
{

    size_t idx = 0;
    game::Evaluation::Eval alpha = -game::Evaluation::Infinity;
    game::Evaluation::Eval beta = game::Evaluation::Infinity;

    for (size_t i = 0; i < m.vsize(); ++i)
    {
        // 局面を進める
        auto back = p.move(m[i]);
        // 先手局面を探索
        auto value = quiesMax(p, alpha, beta, 0);
        // 局面を戻す
        p.undo(back);
        // alpha カット
        if (value <= alpha)
        {
            std::cout << "* " << value << " "
                      << p.string(m[i]) << std::endl;
            return i;
        }
        // 上限を更新
        if (value < beta)
        {
            beta = value;
            idx = i;
            std::cout << "  " << value << " "
                      << p.string(m[i]) << std::endl;
        }
    }

    return idx;
}

/**
 * 先手番局面の静的な評価値を探索により決定する (Max)
 * @parm p     局面
 * @parm alpha 下限値
 * @parm beta  上限値
 * @parm depth 探索の深さ
 * @return 評価値
 */
static game::Evaluation::Eval quiesMax(game::Position &p, game::Evaluation::Eval alpha, game::Evaluation::Eval beta, int depth)
{

    // 評価値の初期値は静的評価値
    // 上限が現在の静的評価値以下の場合探索しない
    game::Evaluation::Eval vmax = p.eval();

    // beta カット (standpat)
    if (beta <= vmax)
    {
        return beta;
    }

    // 末端なので静的評価値を返す
    if (depth > _searchDepth)
    {
        return vmax;
    }

    // 先手番で駒を取る手、王手を回避する手のみを生成 (ほぼ合法手)
    // 該当する手がなかった場合は静的評価値がこの局面の評価値となる
    foundation::Array<game::Move::Move, game::Move::Max> m;
    p.genCaptB(m);

    for (auto move : m)
    {

        // 玉を取る手か
        if (p.dustyB(move))
        {
            vmax = game::Evaluation::Infinity;
            break;
        }

        // 局面を進める
        auto back = p.move(move);
        // 後手番で静止探索
        auto value = quiesMin(p, game::Evaluation::max(vmax, alpha), beta, depth + 1);
        // 局面を戻す
        p.undo(back);

        // beta カット
        if (value >= beta)
        {
            return beta;
        }

        // 下限更新か
        if (value > vmax)
        {
            vmax = value;
        }
    }

    return vmax;
}

/**
 * 後手番局面の静的な評価値を探索により決定する (Min)
 * @parm p     局面
 * @parm alpha 下限値
 * @parm beta  上限値
 * @parm depth 探索の深さ
 * @return 評価値
 */
static game::Evaluation::Eval quiesMin(game::Position &p, game::Evaluation::Eval alpha, game::Evaluation::Eval beta, int depth)
{

    // 評価値の初期値は静的評価値
    // 下限が現在の静的評価値以上の場合探索しない
    game::Evaluation::Eval vmin = p.eval();

    // alpha カット (standpat)
    if (alpha >= vmin)
    {
        return alpha;
    }

    // 末端なので静的評価値を返す
    if (depth > _searchDepth)
    {
        return vmin;
    }

    // 後手番で駒を取る手、王手を回避する手のみを生成 (ほぼ合法手)
    // 該当する手がなかった場合は静的評価値がこの局面の評価値となる
    foundation::Array<game::Move::Move, game::Move::Max> m;
    p.genCaptW(m);

    for (auto move : m)
    {

        // 玉を取る手か
        if (p.dustyW(move))
        {
            vmin = -game::Evaluation::Infinity;
            break;
        }

        // 局面を進める
        auto back = p.move(move);
        // 先手番で静止探索
        auto value = quiesMax(p, alpha, game::Evaluation::min(vmin, beta), depth + 1);
        // 局面を戻す
        p.undo(back);

        // alpha カット
        if (value <= alpha)
        {
            return alpha;
        }

        // 上限更新か
        if (value < vmin)
        {
            vmin = value;
        }
    }

    return vmin;
}

int main(int argc, char *argv[])
{

    // コマンド実行時の引数確認
    if (argc != 4)
    {
        std::cerr << "Usage : "
                  << argv[0]
                  << "<User Name> <Password> <CSA Server>"
                  << std::endl;
        exit(EXIT_FAILURE);
    }

    // 初期化
    game::Shogi::initialize();

    // 駒の価値を設定
    game::Position::setValue(game::Evaluation::Value);
    game::Position::handsValue(game::Evaluation::Hands);

    // CSA サーバに接続
    game::CSAConnection csa(argv[3]);
    csa.connect();

    // CSA サーバにログイン
    csa.login(argv[1], argv[2]);

    // 対局待ち
    auto summary = csa.newGame();

    // 対局受諾
    csa.accept();

    // 局面のインスタンスを作成
    game::Position p(summary);

    // プログラムの手番を確認
    auto myturn = game::Position::myTurn(summary);

    // メッセージ受信用
    foundation::Vector<std::string> message;

    // 対戦
    while (1)
    {

        if (p.turn() == myturn)
        {

            // プログラムの手番

            // 指し手生成
            foundation::Array<game::Move::Move, game::Move::Max> move;
            p.genMove(move);

            // 手が無かったら詰み
            if (move.vsize() == 0)
            {
                // 投了
                csa.resign();
                break;
            }

            // 探索の結果が最も良い手を選択する
            size_t best;
            if (move.vsize() > 1)
            {
                using namespace game;
                if (myturn == Color::Black)
                {
                    best = searchMax(p, move);
                }
                else
                {
                    best = searchMin(p, move);
                }
            }
            else
            {
                // 手がひとつだけしかなかった場合はその手を指すしかない
                best = 0;
            }

            // 指し手送信
            csa.send(p.string(move[best]));

            // 局面を進める
            p.move(move[best]);
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

            // プログラム手番の指し手 (エコー) は無視
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
