/**
 * Search.cpp
 *
 * ネガマックス形式のアルファベータ法 + 反復深化 + 静止探索。
 *
 * ネガマックスでは、評価値を常に「手番側から見た値」(手番側が有利ほど大きい)
 * で扱う。相手にとって良い局面は自分にとって悪い局面なので、子局面の評価値の
 * 符号を反転すれば、先手番でも後手番でも「最大化」だけで探索を書ける。
 *
 * ただし外から見える Result::value は、これまでどおり「先手から見た値」に
 * 戻して返す (Search.h の約束を変えないため)。
 *
 * 時間制限:
 *   - ハードリミット: 思考時間の目安 (timeLimit) を超えたら、探索の途中でも止める。
 *     途中で止まった深さの結果は信用できないので捨て、最後に読み切った深さの
 *     最善手を返す。
 *   - ソフトリミット: 経過時間が目安の SoftLimitRatio 倍を超えていたら、
 *     次の深さを始めない (次の深さは今までの数倍かかり、どうせ途中で止まるため)。
 */

#include <chrono>
#include <vector>
#include <Array.h>
#include "Search.h"

using namespace game;
using namespace foundation;
using namespace Evaluation;
using Clock = std::chrono::steady_clock;

namespace search
{

    using MoveList = Array<Move::Move, Move::Max>;

    static uint64_t _nodes = 0;

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
     * Position::eval() は先手から見た値なので、後手番なら符号を反転する
     */
    static Eval evaluate(Position &p)
    {
        Eval e = p.eval();
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

    /// デルタ枝刈りの余裕 (評価関数が駒割りだけなら 0 でもよいが、念のため)
    static const Eval DeltaMargin = 50;

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
     * 色 by の駒が、マス sq に利いているか
     *
     * 「sq に by の駒が利いている」⇔「sq に相手側の同じ駒を置いたとき、その駒の利きの
     * 中に by の駒がある」(先手と後手の駒の動きは点対称なので)。これを使って、
     * 駒の種類ごとに利きの表 (Effect) とビットボードの AND をとって調べる。
     */
    static bool attacked(const Position &p, Square::Square sq, Color::Color by)
    {
        const Color::Color probe = Color::flip(by);
        const Bitboard &occ = p.occupied();

        // 1 マスだけ動く駒 (馬・竜の縦横・斜め 1 マスもここで調べる)
        const Piece::Piece steppers[] = {
            Piece::FU, Piece::KE, Piece::GI, Piece::KI, Piece::OU,
            Piece::TO, Piece::NY, Piece::NK, Piece::NG, Piece::UM, Piece::RY};
        for (auto k : steppers)
        {
            if (Effect::AD(sq, Piece::polar(k, probe)) & p.bitboard(Piece::polar(k, by)))
            {
                return true;
            }
        }

        // 飛び駒
        if (Effect::KA(sq, occ) & (p.bitboard(Piece::polar(Piece::KA, by)) |
                                   p.bitboard(Piece::polar(Piece::UM, by))))
        {
            return true;
        }
        if (Effect::HI(sq, occ) & (p.bitboard(Piece::polar(Piece::HI, by)) |
                                   p.bitboard(Piece::polar(Piece::RY, by))))
        {
            return true;
        }
        if (Effect::KY(probe, sq, occ) & p.bitboard(Piece::polar(Piece::KY, by)))
        {
            return true;
        }
        return false;
    }

    /**
     * 直前に指した側の玉が取られる状態になっていないか (= 直前の手が合法か)
     *
     * genCapt() はピンされた駒を動かす手など、自玉を取られる非合法手を
     * わずかに生成することがあるので、指した直後にこれで確認して捨てる。
     * @param p 手を指した直後の局面 (手番は相手側)
     */
    static bool lastMoveWasLegal(const Position &p)
    {
        const Color::Color mover = Color::flip(p.turn());
        const Bitboard &king = p.bitboard(Piece::polar(Piece::OU, mover));
        if (!king)
        {
            return true; // 玉がない局面 (詰将棋など)
        }
        return !attacked(p, king.lsb(), p.turn());
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
            auto back = p.move(move);

            // genCapt() の非合法手 (自玉を取られる手) は読まずに捨てる
            if (!inCheck && !lastMoveWasLegal(p))
            {
                p.undo(back);
                continue;
            }

            // デルタ枝刈り: この駒を取っても alpha に届かないなら読まない。
            // ただし王手になる手は詰みにつながるかもしれないので読む。
            if (!inCheck && p.nchecks() == 0)
            {
                // (p.move() の後なので、取った駒・動かした駒は back から求めずに
                //  手を戻してから計算する)
                p.undo(back);
                if (standPat + captureGain(p, move) + DeltaMargin <= alpha)
                {
                    continue;
                }
                back = p.move(move);
            }

            Eval value = -quiescence(p, -beta, -alpha, ply + 1, qply + 1);
            p.undo(back); // 時間切れで戻ってきたときも、必ず先に局面を戻す

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
            auto back = p.move(move);
            // 相手から見た評価値の符号を反転すると自分から見た評価値になる。
            // 窓 [alpha, beta] も、相手から見ると [-beta, -alpha] になる。
            Eval value = -negamax(p, -beta, -alpha, depth - 1, ply + 1);
            p.undo(back); // 時間切れで戻ってきたときも、必ず先に局面を戻す
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
            auto back = p.move(move);
            Eval v = -negamax(p, -beta, -alpha, depth - 1, 1);
            p.undo(back);

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
                 const std::function<void(const Result &)> &onIteration)
    {
        Result result;
        auto start = Clock::now();

        MoveList m;
        p.genMove(m);
        if (m.vsize() == 0)
        {
            return result; // 指せる手がない (詰み)
        }
        result.hasMove = true;
        result.best = m[0]; // 深さ 1 すら読み切れなくても、合法手は必ず返せる
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
