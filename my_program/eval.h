#ifndef MY_EVAL_H
#define MY_EVAL_H

/**
 * eval.h
 *
 * 評価部: 局面の静的評価値を求める。
 *
 *   評価値 = 駒割り (盤上の駒と持ち駒の価値)
 *          + 駒の位置の評価 (駒の種類 × マスごとの点数表)
 *
 * 評価値はすべて「先手から見た値」(先手有利ほど大きい)。
 * 手番に合わせた符号の反転は探索部 (Search) が行う。
 */

#include <Shogi.h>

namespace evaluation
{

    /// 評価部の初期化 (isready のときに 1 回呼ぶ)
    void initialize();

    /// 駒の位置の評価 (先手から見た値)
    game::Evaluation::Eval locationEvaluation(const game::Position &p);

    /// 評価関数: 駒割り + 駒の位置の評価 (先手から見た値)
    game::Evaluation::Eval evaluate(game::Position &p);

}

#endif
