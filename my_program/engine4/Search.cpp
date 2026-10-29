/**
 * Search.cpp
 *
 * 探索部: ネガマックス形式のアルファベータ法 + 反復深化 + 静止探索。
 *
 * ネガマックスでは、評価値を常に「手番側から見た値」(手番側が有利ほど大きい)
 * で扱う。相手にとって良い局面は自分にとって悪い局面なので、子局面の評価値の
 * 符号を反転すれば、先手番でも後手番でも「最大化」だけで探索を書ける。
 * ただし外から見える Result::value と評価関数の戻り値は「先手から見た値」。
 *
 * 時間制限:
 *   - ハードリミット: 思考時間の目安 (timeLimit) を超えたら、探索の途中でも止める。
 *     途中で止まった深さの結果は信用できないので捨て、最後に読み切った深さの
 *     最善手を返す。
 *   - ソフトリミット: 経過時間が目安の SoftLimitRatio 倍を超えていたら、
 *     次の深さを始めない (次の深さは今までの数倍かかり、どうせ途中で止まるため)。
 */

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <Array.h>
#include "Search.h"
#include "eval.h"

using namespace game;
using namespace foundation;
using namespace Evaluation;
using Clock = std::chrono::steady_clock;

namespace search
{

    using MoveList = Array<Move::Move, Move::Max>;

    static uint64_t _nodes = 0;

    /// 評価関数 (think() で受け取ったもの)
    static Evaluator _evaluator = nullptr;

    /// 時間を確認する間隔 (ノード数。2 のべき乗 - 1 にしてビット演算で判定する)
    static const uint64_t CheckInterval = 4096 - 1;

    /// 経過時間がこの割合を超えていたら次の深さを始めない
    static const double SoftLimitRatio = 0.3;

    /// 時間制限があるか
    static bool _useTime = false;

    /// 探索を止める時刻 (ハードリミット)
    static Clock::time_point _deadline;

    /// 時間切れで探索を止めたか
    /// (将来、別スレッドの stop で止めるなら std::atomic<bool> にする)
    static bool _stopped = false;

    static Eval _eval = 0; // 今の局面の評価値 (先手から見た値、手番のボーナスは含めない)

    /// 手を戻すときに必要な情報
    struct Undo
    {
        Move::Move back; // p.move() が返した値 (p.undo() に渡す。取った駒が入っている)
        Eval delta;      // その手で _eval がいくつ増えたか
    };

    /**
     * 手を指して、評価値 _eval も差分で更新する
     * 探索中の p.move() はすべてこれを通す (_eval を書き換えるのはここと undoMove だけ)
     */
    static Undo doMove(Position &p, Move::Move m)
    {
        Undo u;
        u.delta = evaluation::moveDelta(p, m); // 指す「前」に計算する
        u.back = p.move(m);
        _eval += u.delta;
        return u;
    }

    /// doMove() で指した手を戻し、_eval も元に戻す
    static void undoMove(Position &p, const Undo &u)
    {
        p.undo(u.back);
        _eval -= u.delta;
    }

    /**
     * 時間切れかどうかを確認する (CheckInterval + 1 ノードごとに時刻を見る)
     */
    static void checkTime()
    {
        if (_useTime && (_nodes & CheckInterval) == 0 && Clock::now() >= _deadline)
        {
            _stopped = true;
        }
    }

    /**
     * 手番側から見た静的評価値
     * 評価関数は先手から見た値を返すので、後手番なら符号を反転する
     */
    static Eval evaluate(Position &p)
    {
        // 差分で更新してきた値に、手番のボーナスを足すだけ (盤を全部見なくてよい)
        Eval e = _eval + evaluation::turnBonus(p.turn());

#ifdef CHECK_EVAL
        // 確認用: 全部計算し直した値と比べ、違えば止める
        //   CFLAGS=-DCHECK_EVAL make のようにして有効にする (make clean してから)
        Eval full = _evaluator(p);
        if (e != full)
        {
            std::cerr << "差分計算の評価値が違います: 差分 " << e << " / 全計算 " << full
                      << "  " << p.sfen() << std::endl;
            std::abort();
        }
#endif

        return (p.turn() == Color::Black) ? e : -e;
    }

    /**
     * 指せる手がない局面の評価値 (手番側から見た値)
     * 手番側の負け。ただし直前の手が歩打ちなら打ち歩詰め (反則) なので手番側の勝ち。
     * 早く詰ませる手を優先するため、詰みまでの手数 ply を差し引く。
     */
    static Eval noMoveValue(const Position &p, int ply)
    {
        return p.uchifuzume() ? (Infinity - ply) : -(Infinity - ply);
    }

    /**
     * 手の並べ替え: 価値の高い駒を取る手 → 成る手 → その他
     * first が指定されていればその手を先頭にする (前の深さの最善手)
     */
    static void orderMoves(const Position &p, MoveList &m, Move::Move first = Move::None)
    {
        size_t n = m.vsize();
        std::vector<int> score(n);
        for (size_t i = 0; i < n; ++i)
        {
            int captured = absolute(Value[p.square(Move::to(m[i]))]);
            int promote = (m[i] & Move::Promote) ? 1 : 0;
            score[i] = captured * 2 + promote;
            if (first != Move::None && m[i] == first)
            {
                score[i] = 1000000;
            }
        }
        for (size_t i = 1; i < n; ++i)
        {
            Move::Move mv = m[i];
            int s = score[i];
            size_t j = i;
            while (j > 0 && score[j - 1] < s)
            {
                m[j] = m[j - 1];
                score[j] = score[j - 1];
                --j;
            }
            m[j] = mv;
            score[j] = s;
        }
    }

    /// 静止探索の最大手数 (念のための安全装置。通常はここまで続かない)
    static const int QuiesMaxPly = 64;

    /// 静止探索に入ってから何手目までは、王手されたときに逃げ方を全部読むか。
    /// これを超えると王手の連続で探索が爆発するので、静的評価値で打ち切る。
    static const int QuiesCheckPly = 6;

    /// デルタ枝刈りの余裕
    /// captureGain() は駒割りの増分しか見ないので、評価関数に駒の位置などを
    /// 足したときは、その分だけこの値を大きくすること
    static const Eval DeltaMargin = 350; // 50 + 2 × (位置の点数の最大値 150)

    /**
     * 駒を取る手で、評価値 (駒割り) がいくら増えるか
     *  = 取った駒の価値 + 持ち駒が 1 枚増える分 + 成る場合の増分
     * 持ち駒の価値は Evaluation::Hands が枚数に比例している前提で 1 枚目の値を使う。
     */
    static Eval captureGain(const Position &p, Move::Move move)
    {
        Piece::Piece captured = p.square(Move::to(move));
        Eval gain = absolute(Value[captured]);
        if (captured != Piece::EMP)
        {
            // 取った駒は (成駒なら元の駒に戻って) 持ち駒になる
            gain += Hands[1][Piece::neutral(captured)];
        }
        if (move & Move::Promote)
        {
            Piece::Piece pc = p.square(Move::from(move));
            gain += absolute(Value[pc | Piece::Promoted]) - absolute(Value[pc]);
        }
        return gain;
    }

    /**
     * 静止探索 (ネガマックス形式)
     *
     * 深さの末端で、そのまま評価すると「駒を取った直後で、次に取り返される」
     * ような局面を得だと勘違いする (水平線効果)。そこで駒を取る手だけを読み続け、
     * 局面が落ち着いてから評価する。
     *
     *  - 王手されていないとき: 「何もしない」(stand pat) ことができるとみなし、
     *    静的評価値を下限にして、駒を取る手だけを読む。
     *  - 王手されているとき: 何もしないことは許されないので、合法な逃げ方を全部読む。
     *    逃げ方がなければ詰み。
     *
     * @param p     局面
     * @param alpha 手番側がすでに確保している評価値の下限
     * @param beta  手番側の評価値の上限
     * @param ply   ルートからの手数
     * @param qply  静止探索に入ってからの手数
     * @return 手番側から見た評価値
     */
    static Eval quiescence(Position &p, Eval alpha, Eval beta, int ply, int qply)
    {
        ++_nodes;

        // 時間切れなら探索をやめる (返す値は使われない)
        checkTime();
        if (_stopped)
        {
            return 0;
        }

        // 念のための安全装置
        if (ply >= QuiesMaxPly)
        {
            return evaluate(p);
        }

        MoveList m;
        const bool inCheck = (p.nchecks() > 0);
        Eval standPat = 0;

        // 静止探索の奥深くでの王手は、逃げ方を全部読むと探索が爆発するので打ち切る
        if (inCheck && qply >= QuiesCheckPly)
        {
            return evaluate(p);
        }

        if (inCheck)
        {
            // 王手されている: 合法な逃げ方を全部読む
            p.genMove(m);
            if (m.vsize() == 0)
            {
                return noMoveValue(p, ply); // 詰み
            }
        }
        else
        {
            // stand pat: 何もしなくても静的評価値は確保できるとみなす
            standPat = evaluate(p);
            if (standPat >= beta)
            {
                return beta; // beta カット
            }
            if (standPat > alpha)
            {
                alpha = standPat;
            }
            // 駒を取る手だけを生成 (ほぼ合法手。非合法手は下で捨てる)
            p.genCapt(m);
        }

        orderMoves(p, m);

        for (auto move : m)
        {
            Undo u = doMove(p, move);

            // genCapt() が生成する非合法手 (自玉を取られる手) は読まずに捨てる
            if (!inCheck && !p.lastMoveIsLegal())
            {
                undoMove(p, u);
                continue;
            }

            // デルタ枝刈り: この駒を取っても alpha に届かないなら読まない。
            // ただし王手になる手は詰みにつながるかもしれないので読む。
            if (!inCheck && p.nchecks() == 0)
            {
                // (p.move() の後なので、取った駒・動かした駒は back から求めずに
                //  手を戻してから計算する)
                undoMove(p, u);
                if (standPat + captureGain(p, move) + DeltaMargin <= alpha)
                {
                    continue;
                }
                u = doMove(p, move);
            }

            Eval value = -quiescence(p, -beta, -alpha, ply + 1, qply + 1);
            undoMove(p, u); // 時間切れで戻ってきたときも、必ず先に局面を戻す

            if (_stopped)
            {
                return 0;
            }
            if (value >= beta)
            {
                return beta; // beta カット
            }
            if (value > alpha)
            {
                alpha = value;
            }
        }
        return alpha;
    }

    /**
     * ネガマックス形式のアルファベータ探索
     * @param p     局面
     * @param alpha 手番側がすでに確保している評価値の下限
     * @param beta  相手がすでに確保している、手番側の評価値の上限
     * @param depth 残りの探索深さ
     * @param ply   ルートからの手数
     * @return 手番側から見た評価値 (alpha 以下なら alpha、beta 以上なら beta)
     */
    static Eval negamax(Position &p, Eval alpha, Eval beta, int depth, int ply)
    {
        // 末端では静止探索で、局面が落ち着いてから評価する
        if (depth <= 0)
        {
            return quiescence(p, alpha, beta, ply, 0);
        }
        ++_nodes;
        // 時間切れなら探索をやめる (返す値は使われない)
        checkTime();
        if (_stopped)
        {
            return 0;
        }
        MoveList m;
        p.genMove(m);
        // 指せる手がない = 詰み
        if (m.vsize() == 0)
        {
            return noMoveValue(p, ply);
        }
        orderMoves(p, m);
        for (auto move : m)
        {
            Undo u = doMove(p, move);
            // 相手から見た評価値の符号を反転すると自分から見た評価値になる。
            // 窓 [alpha, beta] も、相手から見ると [-beta, -alpha] になる。
            Eval value = -negamax(p, -beta, -alpha, depth - 1, ply + 1);
            undoMove(p, u); // 時間切れで戻ってきたときも、必ず先に局面を戻す
            // 時間切れなら、この局面の結果は信用できないのでそのまま戻る
            if (_stopped)
            {
                return 0;
            }
            // beta カット: 相手はこの局面に来る手を選ばないので打ち切り
            if (value >= beta)
            {
                return beta;
            }
            // 下限を更新
            if (value > alpha)
            {
                alpha = value;
            }
        }
        return alpha;
    }

    /**
     * ルート局面を深さ depth で探索し、最善手と評価値 (手番側から見た値) を返す
     */
    static void searchRoot(Position &p, MoveList &m, int depth,
                           Move::Move &best, Eval &bestValue)
    {
        Eval alpha = -Infinity - 1;
        Eval beta = Infinity + 1;

        best = m[0];
        for (auto move : m)
        {
            Undo u = doMove(p, move);
            Eval v = -negamax(p, -beta, -alpha, depth - 1, 1);
            undoMove(p, u);

            // 時間切れ: この深さの結果は呼び出し側で捨てる
            if (_stopped)
            {
                return;
            }

            if (v > alpha)
            {
                alpha = v;
                best = move;
            }
        }
        bestValue = alpha;
    }

    Result think(Position &p, int maxDepth, double timeLimit,
                 Evaluator evaluator,
                 const std::function<void(const Result &)> &onIteration)
    {
        Result result;
        _evaluator = evaluator;
        auto start = Clock::now();

        // 差分計算の出発点: ルート局面だけは全部計算する (手番のボーナスは含めない)
        _eval = _evaluator(p) - evaluation::turnBonus(p.turn());

        MoveList m;
        p.genMove(m);
        if (m.vsize() == 0)
        {
            return result;
        }
        result.hasMove = true;
        result.best = m[0];
        _nodes = 0;

        // 時間制限の準備 (timeLimit <= 0 なら時間制限なし)
        _stopped = false;
        _useTime = (timeLimit > 0);
        _deadline = start + std::chrono::microseconds(
                                static_cast<long long>(timeLimit * 1000000.0));

        // 先手から見た値に直すための符号
        const Eval sign = (p.turn() == Color::Black) ? 1 : -1;

        // 反復深化: 深さ 1, 2, 3, ... と読んでいく
        for (int depth = 1; depth <= maxDepth; ++depth)
        {

            // 前の深さの最善手を最初に読むと枝刈りが効きやすい
            orderMoves(p, m, result.best);

            Move::Move best;
            Eval value; // 手番側から見た値
            searchRoot(p, m, depth, best, value);

            // 時間切れで途中までしか読めなかった深さの結果は捨てる
            // (前の深さで読み切った result.best をそのまま使う)
            if (_stopped)
            {
                break;
            }

            double sec = std::chrono::duration<double>(
                             Clock::now() - start)
                             .count();
            result.best = best;
            result.value = sign * value; // 外には先手から見た値で返す
            result.depth = depth;
            result.nodes = _nodes;
            result.seconds = sec;
            onIteration(result);

            // 詰みを読み切ったらそれ以上読まない
            if (value >= MateThreshold || value <= -MateThreshold)
            {
                break;
            }
            // ソフトリミット: 次の深さは今までの数倍かかるので、
            // 残り時間で読み切れそうにないなら始めない
            if (_useTime && sec > timeLimit * SoftLimitRatio)
            {
                break;
            }
        }
        return result;
    }

}
