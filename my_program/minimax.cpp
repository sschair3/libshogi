/**
 * minimax.cpp
 *
 * libshogi を使って、minimax 法 (アルファベータ枝刈りなし) で
 * 自己対局するプログラム。
 *
 * 使い方:
 *   ./minimax [探索深さ] [最大手数] [乱数シード]
 *     探索深さ : 何手先まで読むか (既定 3)。4 以上はかなり遅くなる
 *     最大手数 : この手数に達したら対局を打ち切る (既定 300)
 *     乱数シード: 評価値が同じ手が複数あるときの選び方を決める。
 *                 省略するとランダム (毎回違う対局になる)
 *
 * 評価関数は libshogi 標準の駒割り (Position::eval()) で、
 * 値は常に「先手から見た評価値」(先手有利ほど大きい)。
 * そのため先手番は評価値を最大化 (Max)、後手番は最小化 (Min) する。
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

static Eval minimaxMax(Position &p, int depth, int ply);
static Eval minimaxMin(Position &p, int depth, int ply);

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
 * 先手番の局面の評価値を minimax 法で求める (最大化)
 * @parm p     局面
 * @parm depth 残りの探索深さ
 * @parm ply   ルートからの手数
 * @return 評価値
 */
static Eval minimaxMax(Position &p, int depth, int ply)
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

    Eval best = -Infinity - 1;
    for (auto move : m) {
        // 局面を進める
        auto back = p.move(move);
        // 後手番の局面を探索
        Eval value = minimaxMin(p, depth - 1, ply + 1);
        // 局面を戻す
        p.undo(back);

        if (value > best) {
            best = value;
        }
    }
    return best;
}

/**
 * 後手番の局面の評価値を minimax 法で求める (最小化)
 * @parm p     局面
 * @parm depth 残りの探索深さ
 * @parm ply   ルートからの手数
 * @return 評価値
 */
static Eval minimaxMin(Position &p, int depth, int ply)
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

    Eval best = Infinity + 1;
    for (auto move : m) {
        // 局面を進める
        auto back = p.move(move);
        // 先手番の局面を探索
        Eval value = minimaxMax(p, depth - 1, ply + 1);
        // 局面を戻す
        p.undo(back);

        if (value < best) {
            best = value;
        }
    }
    return best;
}

/**
 * ルート局面で最善手を選ぶ
 * 評価値が同じ最善手が複数あるときは乱数で1つ選ぶ。
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
    bool blackToMove = (p.turn() == Color::Black);
    Eval best = blackToMove ? -Infinity - 1 : Infinity + 1;
    std::vector<size_t> candidates;

    for (size_t i = 0; i < m.vsize(); ++i) {
        auto back = p.move(m[i]);
        Eval v = blackToMove ? minimaxMin(p, depth - 1, 1)
                             : minimaxMax(p, depth - 1, 1);
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
    int depth    = (argc > 1) ? std::atoi(argv[1]) : 3;
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
                       "N+minimax_sente\nN-minimax_gote\nPI\n+\n";

    // 千日手判定用: 局面 (ハッシュ) ごとの出現回数
    std::map<Zobrist::key, int> seen;
    seen[p.hash()] = 1;

    std::string result;
    int ply = 0;

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
    std::cout << kifu << std::endl;

    // 棋譜をファイルに保存
    std::ofstream ofs("minimax_kifu.csa");
    ofs << kifu;
    std::cout << "棋譜を minimax_kifu.csa に保存しました" << std::endl;

    return EXIT_SUCCESS;
}
