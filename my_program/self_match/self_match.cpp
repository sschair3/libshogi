#include <Shogi.h>
#include <cstdlib>
#include <iostream>
#include <Array.h>
#include <CSAFile.h>
#include <Position.h>
#include <random>
#include <unistd.h>
#include <fcntl.h>
#include <csignal>
#include <sys/wait.h>
#include <poll.h>
#include <string>
#include <chrono>
#include <thread>
#include <Usi.h>
#include <sstream>
#include <algorithm>
#include <map>
#include <cmath>
#include <iomanip>

using namespace game;
using namespace foundation;
using namespace Evaluation;
using Clock = std::chrono::steady_clock;

enum class Winner
{
    Black,
    White,
    Draw,
    Invalid // 勝ち負けを決められなかった対局 (準備の失敗など)。ShogiHome の「無効対局」
};

struct GameResult
{
    Winner winner;
    std::string reason; // "投了" "詰み" "反則" "時間切れ" "千日手" "最大手数" など
    std::string moves;  // 指した手 (USI 形式)。棋譜の保存や確認に使う
};

#include "EngineProcess.h"

std::string random_play(Position p)
{
    //------------------------------------------------------------/
    // ランダムに16手指した後のsfenを返す関数
    //-----------------------------------------------------------//
    // 対戦
    std::random_device rd;
    std::mt19937 engine(rd()); // メルセンヌ・ツイスタ
    for (int i = 0; i < 16; ++i)
    {
        // 指し手生成
        Array<Move::Move, Move::Max> move;
        p.genMove(move);
        if (move.vsize() == 0)
        {
            std::cerr << "合法手がないため、" << i << "手で終了します。" << std::endl;
            break;
        }
        std::uniform_int_distribution<size_t> distribution(0, move.vsize() - 1);
        size_t random_value = distribution(engine);
        // 着手
        p.move(move[random_value]);
    }
    // play_game は「position sfen <この文字列> moves ...」の形で送るので、SFEN を返す
    return p.sfen();
}

/// 1 局の最大手数 (開始局面から数える)。これに達したら引き分け
static const int MaxPly = 256;

/// bestmove を待つときの、秒読みに足す余裕 (ミリ秒)
static const int MarginMs = 500;

/**
 * 開始局面 startSfen から 1 局指させて、結果を返す
 *   black: 先手のエンジン、white: 後手のエンジン (どちらも usi → usiok まで済ませておく)
 */
GameResult play_game(EngineProcess &black, EngineProcess &white, const std::string &startSfen, int byoyomiMs)
{
    GameResult result;
    result.winner = Winner::Invalid;

    // 盤面は対局プログラム自身も持つ (エンジンの手が合法かを確かめるため)
    Position p;
    if (!p.setSFEN(startSfen))
    {
        result.reason = "開始局面が不正";
        return result; // 無効対局
    }

    // 対局の準備: isready → readyok で、前の対局の読み残し (時間切れの後に
    // 遅れて届いた bestmove など) を読み捨ててから、新しい対局を始める
    for (EngineProcess *e : {&black, &white})
    {
        e->send("isready");
        if (!e->waitFor("readyok", 10000))
        {
            result.reason = "readyok が来ない";
            return result; // 対局を始められなかったので無効対局
        }
        e->send("usinewgame");
    }

    // エンジンに送る局面: 開始局面 + ここまでの手
    std::string position = "position sfen " + startSfen + " moves";

    // 千日手の判定用: 局面のハッシュ値ごとの出現回数
    std::map<Zobrist::key, int> seen;
    seen[p.hash()] = 1;

    // 終局したときの処理: 両方に gameover を送り、結果を返す
    auto finish = [&](Winner winner, const std::string &reason)
    {
        result.winner = winner;
        result.reason = reason;
        bool decided = (winner == Winner::Black || winner == Winner::White);
        black.send(!decided                  ? "gameover draw"
                   : winner == Winner::Black ? "gameover win"
                                             : "gameover lose");
        white.send(!decided                  ? "gameover draw"
                   : winner == Winner::White ? "gameover win"
                                             : "gameover lose");
        return result;
    };

    for (int ply = 1;; ++ply)
    {
        // 1. 手番のエンジンを選ぶ (参照にすると、先手と後手で同じコードを使える)
        bool blackToMove = (p.turn() == Color::Black);
        EngineProcess &engine = blackToMove ? black : white;
        Winner mover = blackToMove ? Winner::Black : Winner::White;    // 今から指す側
        Winner opponent = blackToMove ? Winner::White : Winner::Black; // その相手

        // 2. 局面を送って考えさせる
        engine.send(position);
        engine.send("go byoyomi " + std::to_string(byoyomiMs));

        // 3. bestmove を待つ (info の行は waitForPrefix の中で読み捨てられる)
        std::string line;
        if (!engine.waitForPrefix("bestmove", line, byoyomiMs + MarginMs))
        {
            return finish(opponent, "時間切れ (またはエンジンが終了した)");
        }

        // 4. "bestmove 7g7f ponder 3c3d" から 2 つ目の単語を取り出す
        std::istringstream is(line);
        std::string word, usiMove;
        is >> word >> usiMove;

        if (usiMove == "resign")
        {
            return finish(opponent, "投了");
        }
        if (usiMove == "win")
        {
            // 入玉宣言が正しいかの判定はまだ作っていないので、勝敗を決めず無効対局にする
            return finish(Winner::Invalid, "入玉宣言 (判定は未対応)");
        }

        // 5. 合法な手か確かめて、盤に反映する
        //    fromUSI は合法手の一覧と照らし合わせて、見つかれば m に入れて true を返す
        Move::Move m;
        if (!Usi::fromUSI(p, usiMove, m))
        {
            return finish(opponent, "反則 (" + usiMove + ")");
        }
        p.move(m);
        position += " " + usiMove;
        result.moves += (result.moves.empty() ? "" : " ") + usiMove;

        // 6. 終局の判定
        //    相手に合法手がない = 詰み (今指した側の勝ち)
        Array<Move::Move, Move::Max> replies;
        p.genMove(replies);
        if (replies.vsize() == 0)
        {
            return finish(mover, "詰み");
        }
        //    同じ局面が 4 回目 = 千日手 (連続王手の千日手はまだ区別していない)
        if (++seen[p.hash()] >= 4)
        {
            return finish(Winner::Draw, "千日手");
        }
        if (ply >= MaxPly)
        {
            return finish(Winner::Draw, "最大手数");
        }
    }
}

/// 表示用
static const char *winnerName(Winner w)
{
    return w == Winner::Black   ? "先手の勝ち"
           : w == Winner::White ? "後手の勝ち"
           : w == Winner::Draw  ? "引き分け"
                                : "無効";
}

/* ----------------------- 連続対局の集計 (ShogiHome と同じ) ----------------------- */

/// エンジン 1 つ分の成績
struct PlayerResults
{
    std::string name; // usi で受け取った "id name"
    int win = 0;      // 勝ち数
    int winBlack = 0; // そのうち先手で勝った数
    int winWhite = 0; // そのうち後手で勝った数
};

/// 連続対局全体の成績
struct GameResults
{
    PlayerResults player1; // 1 つ目に指定したエンジン
    PlayerResults player2; // 2 つ目に指定したエンジン
    int draw = 0;          // 引き分け
    int invalid = 0;       // 無効対局
    int total = 0;         // 全対局数
};

/// 1 局の結果を足す (player1IsBlack: その対局で player1 が先手だったか)
static void addResult(GameResults &results, const GameResult &r, bool player1IsBlack)
{
    switch (r.winner)
    {
    case Winner::Black:
    case Winner::White:
    {
        bool player1Won = (r.winner == Winner::Black) == player1IsBlack;
        PlayerResults &winner = player1Won ? results.player1 : results.player2;
        winner.win++;
        if (r.winner == Winner::Black)
            winner.winBlack++;
        else
            winner.winWhite++;
        break;
    }
    case Winner::Draw:
        results.draw++;
        break;
    case Winner::Invalid:
        results.invalid++;
        break;
    }
    results.total++;
}

/// 勝率からレーティング差を求める (勝率 0.5 なら 0、0.64 なら約 +100)
static double eloFromWinRate(double winRate)
{
    return 400.0 * std::log10(winRate / (1.0 - winRate));
}

/// JavaScript の toFixed と同じ表示 (計算できない値は NaN、無限大は Infinity と出す)
static std::string toFixed(double v, int digits)
{
    if (std::isnan(v))
        return "NaN";
    if (std::isinf(v))
        return v > 0 ? "Infinity" : "-Infinity";
    std::ostringstream os;
    os << std::fixed << std::setprecision(digits) << v;
    return os.str();
}

/// ShogiHome の「連続対局終了」と同じ形式で結果を表示する
static void printSummary(const GameResults &results)
{
    const double Z95 = 1.96; // 95% の範囲に入る標準正規分布の値
    const double Z99 = 2.58; // 99%

    // 引き分けと無効対局は除いて、勝ち負けがついた対局だけで計算する
    int n = results.player1.win + results.player2.win;
    int wins = std::max(results.player1.win, results.player2.win); // 勝ち越した側の勝ち数
    double winRate = static_cast<double>(wins) / n;

    // 勝率の 95% 信頼区間の幅と、それをレーティングに直したもの
    double winRateCI = Z95 * std::sqrt(winRate * (1.0 - winRate) / n);
    double rating = eloFromWinRate(winRate);
    double ratingLower = eloFromWinRate(winRate - winRateCI);
    double ratingUpper = eloFromWinRate(winRate + winRateCI);

    // 二項検定 (正規分布で近似): 実力が同じ (勝率 0.5) と仮定したとき、
    // player1 の勝ち数が期待値 n/2 から標準偏差の何倍ずれているか
    double z = std::fabs(results.player1.win - n * 0.5) / std::sqrt(0.5 * 0.5 * n);

    auto player = [](const PlayerResults &pr)
    {
        std::cout << "- " << pr.name << "\n"
                  << "  - 勝ち数: " << pr.win << "\n"
                  << "  - 勝ち数(先手): " << pr.winBlack << "\n"
                  << "  - 勝ち数(後手): " << pr.winWhite << "\n";
    };

    std::cout << "\n連続対局終了\n\n";
    player(results.player1);
    player(results.player2);
    std::cout << "- 引き分け: " << results.draw << "\n"
              << "- 有効対局数: " << results.total - results.invalid << "\n"
              << "- 無効対局数: " << results.invalid << "\n"
              << "- レーティング差 (引き分け無効)\n"
              << "  - " << toFixed(rating, 2) << "\n"
              << "  - 95% CI: [" << toFixed(ratingLower, 1) << ", " << toFixed(ratingUpper, 1) << "]\n"
              << "- 二項検定\n"
              << "  - np > 5: " << (n > 10 ? "True" : "False") << "\n" // n × 0.5 > 5 と同じ
              << "  - Z値: " << toFixed(z, 2) << "\n"
              << "  - 有意水準5%: " << (z > Z95 ? "True" : "False") << "\n"
              << "  - 有意水準1%: " << (z > Z99 ? "True" : "False") << std::endl;
}

/// usi を送って usiok まで読み、途中の "id name ..." からエンジン名を取り出す
static bool usiHandshake(EngineProcess &engine, std::string &name, int timeoutMs)
{
    engine.send("usi");
    std::string line;
    while (engine.readLine(line, timeoutMs))
    {
        if (line.rfind("id name ", 0) == 0)
        {
            name = line.substr(8); // "id name " の 8 文字の後ろ
        }
        if (line == "usiok")
        {
            return true;
        }
    }
    return false;
}

/// 自作エンジンの Depth の上限 (engine1〜4 の options() の max と同じ)
static const int MaxDepth = 64;

int main()
{
    std::string engine1_path, engine2_path;
    std::cout << "エンジンパスを２つ指定してください。" << std::endl;
    std::cin >> engine1_path >> engine2_path;
    if (engine1_path == "" || engine2_path == "")
    {
        std::cout << "プログラムを終了します。" << std::endl;
        return 0;
    }

    // 深さと時間の両方で制限できる。先に来たほうで探索が止まる
    //   深さ 0         : 深さで制限しない (時間だけで止める)
    //   秒読み (ミリ秒): 1 手あたりの持ち時間。エンジンはこの時間内に指す
    int depth_in = 0, byoyomiMs = 0, gamePairs = 0;
    std::cout << "探索の深さを入力してください (0 なら深さで制限しない)。" << std::endl;
    std::cin >> depth_in;
    std::cout << "1 手の秒読みをミリ秒で入力してください (例: 1000)。" << std::endl;
    std::cin >> byoyomiMs;
    std::cout << "対局の組数を入力してください (1 組 = 先後を入れ替えた 2 局)。" << std::endl;
    std::cin >> gamePairs;
    if (!std::cin || depth_in < 0 || byoyomiMs <= 0 || gamePairs <= 0)
    {
        std::cout << "入力が正しくありません。プログラムを終了します。" << std::endl;
        return 0;
    }

    // 深さ 0 (制限しない) のとき:
    //   自作エンジンの Depth は 0 を受け付けないので上限の 64 を送る
    //   やねうら王の DepthLimit は 0 が「制限なし」の意味
    std::string depth = std::to_string(depth_in == 0 ? MaxDepth : depth_in);
    std::string depthLimit = std::to_string(depth_in);
    std::cout << "深さ " << (depth_in == 0 ? "制限なし" : depth)
              << " / 秒読み " << byoyomiMs << " ミリ秒 / " << gamePairs * 2 << " 局" << std::endl;
    signal(SIGPIPE, SIG_IGN);
    // 初期化
    Shogi::initialize();
    // 局面のインスタンスを作成
    Position p;
    p.init();
    EngineProcess engine1;
    EngineProcess engine2;
    if (!engine1.start(engine1_path) || !engine2.start(engine2_path))
    {
        return 1;
    }

    GameResults results;
    for (auto pair : {std::make_pair(&engine1, &results.player1), std::make_pair(&engine2, &results.player2)})
    {
        if (!usiHandshake(*pair.first, pair.second->name, 3000))
        {
            std::cerr << "usiok が来ない" << std::endl;
            return 1;
        }
        pair.first->send("setoption name Depth value " + depth);
        pair.first->send("setoption name DepthLimit value " + depthLimit); // やねうら王用
        // pair.first->send("setoption name EvalDir value /Users/shogo/shogi/libshogi/my_program/yaneuraou-hao/eval");
    }

    const int totalGames = gamePairs * 2;
    for (int i = 0; i < gamePairs; i++)
    {
        std::string initial_sfen = random_play(p);

        // 同じ開始局面で、先後を入れ替えて 2 局指す
        for (bool player1IsBlack : {true, false})
        {
            EngineProcess &black = player1IsBlack ? engine1 : engine2;
            EngineProcess &white = player1IsBlack ? engine2 : engine1;
            const std::string &blackName = player1IsBlack ? results.player1.name : results.player2.name;
            const std::string &whiteName = player1IsBlack ? results.player2.name : results.player1.name;

            GameResult r = play_game(black, white, initial_sfen, byoyomiMs);
            addResult(results, r, player1IsBlack);

            std::cout << "[" << results.total << "/" << totalGames << "] "
                      << "先手 " << blackName << " vs 後手 " << whiteName << ": "
                      << winnerName(r.winner) << " (" << r.reason << ")" << std::endl;
        }
    }

    printSummary(results);
    engine1.quit();
    engine2.quit();
    return 0;
}
