#include <cstdlib>
#include <iostream>
#include <Shogi.h>
#include <Array.h>
#include <string>
#include <CSAConnection.h>
#include <random>
#include <fstream>

using namespace game;
using namespace foundation;
using namespace Evaluation;

// 最大の探索深さ
static int _searchDepth = 11;

static size_t searchMax(Position &, Array<Move::Move, Move::Max> &);
static size_t searchMin(Position &, Array<Move::Move, Move::Max> &);
static Eval quiesMax(Position &, Eval alpha, Eval beta, int depth);
static Eval quiesMin(Position &, Eval alpha, Eval beta, int depth);

/**
 * 先手最善手を調べる
 * @parm p 局面
 * @parm m 候補手
 * @return 最善手のインデックス
 */
static size_t searchMax(game::Position &p, foundation::Array<Move::Move, Move::Max> &m)
{

    size_t idx = 0;
    Eval alpha = -Infinity;
    Eval beta = Infinity;

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
}

/**
 * 後手最善手を調べる
 * @parm p 局面
 * @parm m 候補手
 * @return 最善手のインデックス
 */
static size_t searchMin(Position &p, Array<Move::Move, Move::Max> &m)
{

    size_t idx = 0;
    Eval alpha = -Infinity;
    Eval beta = Infinity;

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
static Eval quiesMax(Position &p, Eval alpha, Eval beta, int depth)
{

    // 評価値の初期値は静的評価値
    // 上限が現在の静的評価値以下の場合探索しない
    Eval vmax = p.eval();

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
    Array<Move::Move, Move::Max> m;
    p.genCaptB(m);

    for (auto move : m)
    {

        // 玉を取る手か
        if (p.dustyB(move))
        {
            vmax = Infinity;
            break;
        }

        // 局面を進める
        auto back = p.move(move);
        // 後手番で静止探索
        auto value = quiesMin(p, max(vmax, alpha), beta, depth + 1);
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
static Eval quiesMin(Position &p, Eval alpha, Eval beta, int depth)
{

    // 評価値の初期値は静的評価値
    // 下限が現在の静的評価値以上の場合探索しない
    Eval vmin = p.eval();

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
    Array<Move::Move, Move::Max> m;
    p.genCaptW(m);

    for (auto move : m)
    {

        // 玉を取る手か
        if (p.dustyW(move))
        {
            vmin = -Infinity;
            break;
        }

        // 局面を進める
        auto back = p.move(move);
        // 先手番で静止探索
        auto value = quiesMax(p, alpha, min(vmin, beta), depth + 1);
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
    bool teban = Color::Black;
    teban = !teban;
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
        // std::random_device rd;
        // std::mt19937 gen(rd());
        // std::cout << rd() << std::endl;
        // int choice = gen() % (int)move.vsize();

        size_t best;
        if (move.vsize() > 1)
        {
            if (teban)
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

        // 手を表示
        std::cout << "最善手：" << p.string(move[best]) << std::endl;
        kifu += p.string(move[best]) + "\n";

        // 着手
        p.move(move[best]);

        // 局面を表示
        std::cout << p << std::endl;
        teban = !teban;
    }

    std::cout << kifu << "\n";

    return 0;
}
