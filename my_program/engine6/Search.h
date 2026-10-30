#ifndef MY_SEARCH_H
#define MY_SEARCH_H

/**
 * Search.h
 *
 * 探索部: ネガマックス形式のアルファベータ法 + 反復深化 + 静止探索。
 *
 * 評価関数は探索部には含めず、think() の引数として外 (engine.cpp) から渡す。
 * 評価関数を差し替えるときに探索部を書き換えずに済むようにするため。
 */

#include <cstdint>
#include <functional>
#include <Shogi.h>

namespace search
{

    /**
     * 評価関数の型
     * 局面の静的評価値を「先手から見た値」(先手有利ほど大きい) で返す。
     */
    using Evaluator = game::Evaluation::Eval (*)(game::Position &);

    /// 探索結果
    struct Result
    {
        bool hasMove = false;                     ///< 指せる手があったか
        game::Move::Move best = game::Move::None; ///< 最善手
        game::Evaluation::Eval value = 0;         ///< 評価値 (先手から見た値)
        int depth = 0;                            ///< 読み切った深さ
        uint64_t nodes = 0;                       ///< 探索したノード数
        double seconds = 0.0;                     ///< 経過時間 (秒)
    };

    /**
     * 反復深化で最善手を探す
     * @param p           局面 (探索中に動かすが、終わると元に戻っている)
     * @param maxDepth    最大の探索深さ
     * @param timeLimit   思考時間の目安 (秒)。0 以下なら時間制限なし
     * @param evaluator   評価関数
     * @param onIteration 1 つの深さを読み切るたびに呼ばれる (info 出力用)
     * @return 探索結果 (最後に読み切った深さの結果)
     */
    Result think(game::Position &p, int maxDepth, double timeLimit,
                 Evaluator evaluator,
                 const std::function<void(const Result &)> &onIteration);

}

#endif
