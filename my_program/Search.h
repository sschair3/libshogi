#ifndef MY_SEARCH_H
#define MY_SEARCH_H

/**
 * Search.h
 *
 * アルファベータ法 + 反復深化による探索 (alphabeta.cpp の探索部分を
 * USI エンジンから使えるように切り出したもの)。
 */

#include <cstdint>
#include <functional>
#include <Shogi.h>

namespace search {

/// 探索結果
struct Result {
    bool                     hasMove = false;               ///< 指せる手があったか
    game::Move::Move         best    = game::Move::None;    ///< 最善手
    game::Evaluation::Eval   value   = 0;                   ///< 評価値 (先手から見た値)
    int                      depth   = 0;                   ///< 探索し終えた深さ
    uint64_t                 nodes   = 0;                   ///< 探索したノード数
    double                   seconds = 0.0;                 ///< 経過時間 (秒)
};

/// 詰みの評価値の目安 (|value| がこれ以上なら詰みを読み切っている)
const game::Evaluation::Eval MateThreshold = game::Evaluation::Infinity - 1000;

/**
 * 反復深化で最善手を探す
 * @param p         局面 (探索中に動かすが、終わると元に戻っている)
 * @param maxDepth  最大の探索深さ
 * @param timeLimit 思考時間の目安 (秒)。0 以下なら時間制限なし
 * @param onIteration 1 つの深さを読み終えるたびに呼ばれる (info 出力用)
 * @return 探索結果
 */
Result think(game::Position &p, int maxDepth, double timeLimit,
             const std::function<void(const Result &)> &onIteration);

}

#endif
