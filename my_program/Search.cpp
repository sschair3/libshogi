/**
 * Search.cpp
 *
 * アルファベータ法 + 反復深化。評価値は常に「先手から見た値」で、
 * 先手番は最大化 (Max)、後手番は最小化 (Min) する。
 */

#include <chrono>
#include <vector>
#include <Array.h>
#include "Search.h"

using namespace game;
using namespace foundation;
using namespace Evaluation;

namespace search {

using MoveList = Array<Move::Move, Move::Max>;

static uint64_t _nodes = 0;

static Eval alphabetaMax(Position &p, Eval alpha, Eval beta, int depth, int ply);
static Eval alphabetaMin(Position &p, Eval alpha, Eval beta, int depth, int ply);

/**
 * 指せる手がない局面の評価値 (手番側の負け。直前が歩打ちなら打ち歩詰めで手番側の勝ち)
 */
static Eval noMoveValue(const Position &p, int ply)
{
    bool blackToMove    = (p.turn() == Color::Black);
    bool sideToMoveWins = p.uchifuzume();
    bool blackWins      = (blackToMove == sideToMoveWins);
    return blackWins ? (Infinity - ply) : -(Infinity - ply);
}

/**
 * 手の並べ替え: 価値の高い駒を取る手 → 成る手 → その他
 * first が指定されていればその手を先頭にする (前の深さの最善手)
 */
static void orderMoves(const Position &p, MoveList &m, Move::Move first = Move::None)
{
    size_t n = m.vsize();
    std::vector<int> score(n);
    for (size_t i = 0; i < n; ++i) {
        int captured = absolute(Value[p.square(Move::to(m[i]))]);
        int promote  = (m[i] & Move::Promote) ? 1 : 0;
        score[i] = captured * 2 + promote;
        if (first != Move::None && m[i] == first) {
            score[i] = 1000000;
        }
    }
    for (size_t i = 1; i < n; ++i) {
        Move::Move mv = m[i];
        int s = score[i];
        size_t j = i;
        while (j > 0 && score[j - 1] < s) {
            m[j] = m[j - 1];
            score[j] = score[j - 1];
            --j;
        }
        m[j] = mv;
        score[j] = s;
    }
}

/// 先手番の局面を探索 (最大化)
static Eval alphabetaMax(Position &p, Eval alpha, Eval beta, int depth, int ply)
{
    ++_nodes;
    if (depth == 0) {
        return p.eval();
    }
    MoveList m;
    p.genMove(m);
    if (m.vsize() == 0) {
        return noMoveValue(p, ply);
    }
    orderMoves(p, m);
    for (auto move : m) {
        auto back = p.move(move);
        Eval value = alphabetaMin(p, alpha, beta, depth - 1, ply + 1);
        p.undo(back);
        if (value >= beta) {
            return beta;        // beta カット
        }
        if (value > alpha) {
            alpha = value;
        }
    }
    return alpha;
}

/// 後手番の局面を探索 (最小化)
static Eval alphabetaMin(Position &p, Eval alpha, Eval beta, int depth, int ply)
{
    ++_nodes;
    if (depth == 0) {
        return p.eval();
    }
    MoveList m;
    p.genMove(m);
    if (m.vsize() == 0) {
        return noMoveValue(p, ply);
    }
    orderMoves(p, m);
    for (auto move : m) {
        auto back = p.move(move);
        Eval value = alphabetaMax(p, alpha, beta, depth - 1, ply + 1);
        p.undo(back);
        if (value <= alpha) {
            return alpha;       // alpha カット
        }
        if (value < beta) {
            beta = value;
        }
    }
    return beta;
}

/**
 * ルート局面を深さ depth で探索し、最善手と評価値を返す
 */
static void searchRoot(Position &p, MoveList &m, int depth,
                       Move::Move &best, Eval &bestValue)
{
    const Eval Lower = -Infinity - 1;
    const Eval Upper =  Infinity + 1;
    bool blackToMove = (p.turn() == Color::Black);
    Eval alpha = Lower;
    Eval beta  = Upper;

    best = m[0];
    for (auto move : m) {
        auto back = p.move(move);
        Eval v = blackToMove ? alphabetaMin(p, alpha, beta, depth - 1, 1)
                             : alphabetaMax(p, alpha, beta, depth - 1, 1);
        p.undo(back);
        if (blackToMove && v > alpha) {
            alpha = v;
            best  = move;
        }
        if (!blackToMove && v < beta) {
            beta = v;
            best = move;
        }
    }
    bestValue = blackToMove ? alpha : beta;
}

Result think(Position &p, int maxDepth, double timeLimit,
             const std::function<void(const Result &)> &onIteration)
{
    Result result;
    auto start = std::chrono::steady_clock::now();

    MoveList m;
    p.genMove(m);
    if (m.vsize() == 0) {
        return result;          // 指せる手がない (詰み)
    }
    result.hasMove = true;
    result.best    = m[0];
    _nodes = 0;

    // 反復深化: 深さ 1, 2, 3, ... と読んでいく
    for (int depth = 1; depth <= maxDepth; ++depth) {

        // 前の深さの最善手を最初に読むと枝刈りが効きやすい
        orderMoves(p, m, result.best);

        Move::Move best;
        Eval value;
        searchRoot(p, m, depth, best, value);

        double sec = std::chrono::duration<double>(
                         std::chrono::steady_clock::now() - start).count();
        result.best    = best;
        result.value   = value;
        result.depth   = depth;
        result.nodes   = _nodes;
        result.seconds = sec;
        onIteration(result);

        // 詰みを読み切ったらそれ以上読まない
        if (value >= MateThreshold || value <= -MateThreshold) {
            break;
        }
        // 次の深さはおおよそ数倍の時間がかかるので、時間が足りなそうならやめる
        // (探索の途中では止められないため、早めに打ち切る)
        if (timeLimit > 0 && sec * 5 > timeLimit) {
            break;
        }
    }
    return result;
}

}
