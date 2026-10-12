/**
 * alphabeta.cpp
 *
 * libshogi を使って、アルファベータ法で自己対局するプログラム。
 * minimax.cpp と同じ評価関数・同じ乱数の使い方なので、同じ引数で
 * 実行すると minimax.cpp と全く同じ対局になり、探索ノード数だけが
 * 減る (アルファベータ法は minimax 法と同じ答えをより少ない探索で出す)。
 *
 * 使い方:
 *   ./alphabeta [探索深さ] [最大手数] [乱数シード]
 *     探索深さ : 何手先まで読むか (既定 4)
 *     最大手数 : この手数に達したら対局を打ち切る (既定 300)
 *     乱数シード: 評価値が同じ手が複数あるときの選び方を決める。
 *                 省略するとランダム (毎回違う対局になる)
 *
 * 評価関数は libshogi 標準の駒割り (Position::eval()) で、
 * 値は常に「先手から見た評価値」(先手有利ほど大きい)。
 * 先手番は評価値を最大化 (Max)、後手番は最小化 (Min) する。
 *
 *   alpha : 先手 (Max) がすでに確保している評価値の下限
 *   beta  : 後手 (Min) がすでに確保している評価値の上限
 *   alpha >= beta になったら、その局面はどちらかが選ばないので
 *   残りの手は読まなくてよい (枝刈り)。
 */

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <random>
#include <chrono>

#include <Shogi.h>
#include <Array.h>

using namespace game;
using namespace foundation;
using namespace Evaluation;

using MoveList = Array<Move::Move, Move::Max>;

// 探索したノード数 (統計表示用)
static uint64_t _nodes = 0;

static Eval alphabetaMax(Position &p, Eval alpha, Eval beta, int depth, int ply);
static Eval alphabetaMin(Position &p, Eval alpha, Eval beta, int depth, int ply);

/**
 * 指せる手がない局面の評価値 (手番側の負け)
 * 直前の手が歩打ちなら打ち歩詰め (反則) なので、逆に手番側の勝ち。
 * 早く詰ませる手を優先するため、詰みまでの手数 ply を差し引く。
 * @parm p   局面
 * @parm ply ルートからの手数
 * @return 先手から見た評価値
 */
static Eval noMoveValue(const Position &p, int ply)
{
    bool blackToMove = (p.turn() == Color::Black);
    bool sideToMoveWins = p.uchifuzume();
    bool blackWins = (blackToMove == sideToMoveWins);
    return blackWins ? (Infinity - ply) : -(Infinity - ply);
}

/**
 * 手の並べ替え (move ordering)
 * アルファベータ法は「良い手から先に読む」ほど枝刈りが多く起きる。
 * ここでは簡単に、価値の高い駒を取る手 → 成る手 → その他 の順にする。
 * @parm p 局面
 * @parm m 指し手 (並べ替えられる)
 */
static void orderMoves(const Position &p, MoveList &m)
{
    size_t n = m.vsize();
    std::vector<int> score(n);
    for (size_t i = 0; i < n; ++i) {
        // 取られる駒の価値 (駒打ちや空きマスへの移動なら 0)
        int captured = absolute(Value[p.square(Move::to(m[i]))]);
        int promote  = (m[i] & Move::Promote) ? 1 : 0;
        score[i] = captured * 2 + promote;
    }
    // 挿入ソート (スコアの大きい順。同点なら元の順番を保つ)
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

/**
 * 先手番の局面をアルファベータ法で探索する (最大化)
 * @parm p     局面
 * @parm alpha 下限値
 * @parm beta  上限値
 * @parm depth 残りの探索深さ
 * @parm ply   ルートからの手数
 * @return 評価値 (alpha 以下なら alpha、beta 以上なら beta を返す)
 */
static Eval alphabetaMax(Position &p, Eval alpha, Eval beta, int depth, int ply)
{
    ++_nodes;

    // 末端なので静的評価値を返す
    if (depth == 0) {
        return p.eval();
    }

    MoveList m;
    p.genMove(m);

    // 指せる手がない = 詰み
    if (m.vsize() == 0) {
        return noMoveValue(p, ply);
    }

    orderMoves(p, m);

    for (auto move : m) {
        // 局面を進める
        auto back = p.move(move);
        // 後手番の局面を探索
        Eval value = alphabetaMin(p, alpha, beta, depth - 1, ply + 1);
        // 局面を戻す
        p.undo(back);

        // beta カット: 後手はこの局面に来る手を選ばないので打ち切り
        if (value >= beta) {
            return beta;
        }
        // 下限を更新
        if (value > alpha) {
            alpha = value;
        }
    }
    return alpha;
}

/**
 * 後手番の局面をアルファベータ法で探索する (最小化)
 * @parm p     局面
 * @parm alpha 下限値
 * @parm beta  上限値
 * @parm depth 残りの探索深さ
 * @parm ply   ルートからの手数
 * @return 評価値 (alpha 以下なら alpha、beta 以上なら beta を返す)
 */
static Eval alphabetaMin(Position &p, Eval alpha, Eval beta, int depth, int ply)
{
    ++_nodes;

    // 末端なので静的評価値を返す
    if (depth == 0) {
        return p.eval();
    }

    MoveList m;
    p.genMove(m);

    // 指せる手がない = 詰み
    if (m.vsize() == 0) {
        return noMoveValue(p, ply);
    }

    orderMoves(p, m);

    for (auto move : m) {
        // 局面を進める
        auto back = p.move(move);
        // 先手番の局面を探索
        Eval value = alphabetaMax(p, alpha, beta, depth - 1, ply + 1);
        // 局面を戻す
        p.undo(back);

        // alpha カット: 先手はこの局面に来る手を選ばないので打ち切り
        if (value <= alpha) {
            return alpha;
        }
        // 上限を更新
        if (value < beta) {
            beta = value;
        }
    }
    return beta;
}

/**
 * ルート局面で最善手を選ぶ
 * 評価値が同じ最善手が複数あるときは乱数で1つ選ぶ。
 * 同点の手も見つけられるよう、窓を「今の最善値と同じ値」まで
 * 含むように 1 だけ広げて探索している。
 * @parm p     局面
 * @parm m     候補手 (合法手)
 * @parm depth 探索深さ
 * @parm rng   乱数生成器
 * @parm value 最善手の評価値を受け取る
 * @return 最善手のインデックス
 */
static size_t searchRoot(Position &p, MoveList &m, int depth,
                         std::mt19937 &rng, Eval &value)
{
    const Eval Lower = -Infinity - 1;
    const Eval Upper =  Infinity + 1;

    bool blackToMove = (p.turn() == Color::Black);
    Eval best = blackToMove ? Lower : Upper;
    std::vector<size_t> candidates;

    for (size_t i = 0; i < m.vsize(); ++i) {
        auto back = p.move(m[i]);
        Eval v;
        if (blackToMove) {
            // best 未満なら興味がない → alpha = best - 1
            Eval alpha = candidates.empty() ? Lower : best - 1;
            v = alphabetaMin(p, alpha, Upper, depth - 1, 1);
        } else {
            // best より大きければ興味がない → beta = best + 1
            Eval beta = candidates.empty() ? Upper : best + 1;
            v = alphabetaMax(p, Lower, beta, depth - 1, 1);
        }
        p.undo(back);

        bool better = blackToMove ? (v > best) : (v < best);
        if (better) {
            best = v;
            candidates.clear();
        }
        if (v == best) {
            candidates.push_back(i);
        }
    }

    value = best;
    std::uniform_int_distribution<size_t> dist(0, candidates.size() - 1);
    return candidates[dist(rng)];
}

int main(int argc, char *argv[])
{
    // 実行時引数
    int depth    = (argc > 1) ? std::atoi(argv[1]) : 4;
    int maxPlies = (argc > 2) ? std::atoi(argv[2]) : 300;
    unsigned int seed = (argc > 3) ? static_cast<unsigned int>(std::atoi(argv[3]))
                                   : std::random_device()();
    if (depth < 1) {
        std::cerr << "探索深さは 1 以上を指定してください" << std::endl;
        return EXIT_FAILURE;
    }
    std::mt19937 rng(seed);

    // 初期化
    Shogi::initialize();

    // 駒の価値を設定
    Position::setValue(Evaluation::Value);
    Position::handsValue(Evaluation::Hands);

    // 平手初期局面
    Position p;
    p.init();

    std::cout << "探索深さ: " << depth << "  最大手数: " << maxPlies
              << "  乱数シード: " << seed << std::endl;
    std::cout << p << std::endl;

    // CSA 形式の棋譜
    std::string kifu = "'CSA encoding=UTF-8\nV3.0\n"
                       "N+alphabeta_sente\nN-alphabeta_gote\nPI\n+\n";

    // 千日手判定用: 局面 (ハッシュ) ごとの出現回数
    std::map<Zobrist::key, int> seen;
    seen[p.hash()] = 1;

    std::string result;
    int ply = 0;
    uint64_t totalNodes = 0;
    double totalSec = 0.0;

    // 対局
    while (1) {

        // 指し手生成
        MoveList move;
        p.genMove(move);

        // 手がなかったら詰み (直前が歩打ちなら打ち歩詰めで反則負け)
        if (move.vsize() == 0) {
            bool blackToMove = (p.turn() == Color::Black);
            if (p.uchifuzume()) {
                result = std::string(blackToMove ? "後手" : "先手")
                       + "の打ち歩詰め (反則) で" + (blackToMove ? "先手" : "後手") + "の勝ち";
                kifu += "%ILLEGAL_MOVE\n";
            } else {
                result = std::string(blackToMove ? "後手" : "先手") + "の勝ち (詰み)";
                kifu += "%TORYO\n";
            }
            break;
        }

        // 手数制限
        if (ply >= maxPlies) {
            result = std::to_string(maxPlies) + " 手に達したので打ち切り";
            kifu += "%CHUDAN\n";
            break;
        }

        // 最善手を探索
        _nodes = 0;
        Eval value = 0;
        auto start = std::chrono::steady_clock::now();
        size_t best = searchRoot(p, move, depth, rng, value);
        double sec = std::chrono::duration<double>(
                         std::chrono::steady_clock::now() - start).count();
        totalNodes += _nodes;
        totalSec   += sec;

        // 手を表示
        std::string str = p.string(move[best]);
        ++ply;
        std::cout << ply << "手目 " << str
                  << "  評価値(先手から): " << value
                  << "  ノード数: " << _nodes
                  << "  時間: " << sec << "秒" << std::endl;
        kifu += str + "\n";

        // 着手
        p.move(move[best]);

        // 局面を表示
        std::cout << p << std::endl;

        // 千日手 (同一局面 4 回) なら引き分け
        // ※ 連続王手の千日手 (王手をかけ続けた側の負け) は判定していない
        if (++seen[p.hash()] >= 4) {
            result = "千日手で引き分け";
            kifu += "%SENNICHITE\n";
            break;
        }
    }

    std::cout << "終局: " << result << " (" << ply << "手)" << std::endl;
    std::cout << "総ノード数: " << totalNodes << "  総探索時間: "
              << totalSec << "秒" << std::endl;
    std::cout << kifu << std::endl;

    // 棋譜をファイルに保存
    std::ofstream ofs("alphabeta_kifu.csa");
    ofs << kifu;
    std::cout << "棋譜を alphabeta_kifu.csa に保存しました" << std::endl;

    return EXIT_SUCCESS;
}
