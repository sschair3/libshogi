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

using namespace game;
using namespace foundation;
using namespace Evaluation;
using Clock = std::chrono::steady_clock;

enum class Winner
{
    Black,
    White,
    Draw
};

struct GameResult
{
    Winner winner;
    std::string reason; // "投了" "詰み" "反則" "時間切れ" "千日手" "最大手数" など
    std::string moves;  // 指した手 (USI 形式)。棋譜の保存や確認に使う
};

class EngineProcess
{
private:
    pid_t _pid = -1;     // 子プロセスの番号 (waitpid / kill に使う)
    int _toEngine = -1;  // 親が「書く」端 (管A の入口)
    std::string _buffer; // read() で読んだけれど、まだ1行になっていない分
public:
    int _fromEngine = -1; // 親が「読む」端 (管B の出口)
    bool start(const std::string &path)
    {
        // 実行できるファイルか先に確かめる
        // (exec の失敗は子プロセスの中で起きるので、親からは分かりにくいため)
        if (access(path.c_str(), X_OK) != 0)
        {
            std::cerr << "エンジンを実行できません: " << path << std::endl;
            return false;
        }

        // 管を 2 本作る。[0] が出口 (読む端)、[1] が入口 (書く端)
        int toEngine[2];   // 管A: 親 → エンジン
        int fromEngine[2]; // 管B: エンジン → 親
        if (pipe(toEngine) != 0)
        {
            return false;
        }
        if (pipe(fromEngine) != 0)
        {
            close(toEngine[0]);
            close(toEngine[1]);
            return false;
        }

        _pid = fork(); // ここでプログラムが親と子の 2 つに分かれる
        if (_pid < 0)
        {
            close(toEngine[0]);
            close(toEngine[1]);
            close(fromEngine[0]);
            close(fromEngine[1]);
            return false;
        }

        if (_pid == 0)
        {
            // ---- 子: 標準入出力を管につなぎ替えて、エンジンに変身する ----
            dup2(toEngine[0], 0);   // 0番 (cin)  ← 管A の出口
            dup2(fromEngine[1], 1); // 1番 (cout) → 管B の入口

            // つなぎ替えたので、元の 4 つの端はもう要らない
            close(toEngine[0]);
            close(toEngine[1]);
            close(fromEngine[0]);
            close(fromEngine[1]);

            execl(path.c_str(), path.c_str(), static_cast<char *>(nullptr));
            _exit(127); // ここに来たら exec に失敗している
        }

        // ---- 親: 自分が使う端だけ残す ----
        close(toEngine[0]);   // 管A の出口はエンジンが使う
        close(fromEngine[1]); // 管B の入口はエンジンが使う
        _toEngine = toEngine[1];
        _fromEngine = fromEngine[0];

        // 2 つ目のエンジンを fork したとき、1 つ目のエンジンの管の端が
        // 2 つ目のエンジンに引き継がれないようにする (exec のときに自動で閉じる)。
        // これがないと、1 つ目のエンジンに EOF が届かず終わらなくなることがある
        fcntl(_toEngine, F_SETFD, FD_CLOEXEC);
        fcntl(_fromEngine, F_SETFD, FD_CLOEXEC);

        return true;
    }
    void send(const std::string &line)
    {
        std::string s = line + "\n";
        write(_toEngine, s.data(), s.size());
    }
    bool readLine(std::string &line, int timeoutMs)
    {
        auto deadline = Clock::now() + std::chrono::milliseconds(timeoutMs);
        while (true)
        {
            if (_buffer.find('\n') != std::string::npos)
            {
                line = _buffer.substr(0, _buffer.find('\n'));
                _buffer.erase(0, _buffer.find("\n") + 1);
                if (!line.empty() && line.back() == '\r')
                {
                    line.pop_back(); // \r\n 対策
                }
                return true;
            }
            auto rest = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
            if (rest <= 0)
            {
                return false;
            }
            pollfd pfd;
            pfd.fd = _fromEngine; // この番号を見張る
            pfd.events = POLLIN;  // 「読めるデータが来たか」を見張る
            int r = poll(&pfd, 1, static_cast<int>(rest));
            if (r <= 0)
            {
                return false;
            }
            char buf[4096];
            ssize_t n = read(_fromEngine, buf, sizeof buf);
            if (n <= 0)
            {
                return false;
            }
            _buffer.append(buf, static_cast<size_t>(n)); // n バイトだけ足す
        }
    }
    bool waitFor(const std::string &word, int timeoutMs)
    {
        auto deadline = Clock::now() + std::chrono::milliseconds(timeoutMs);
        std::string line;
        while (true)
        {
            auto rest = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
            if (rest <= 0 || !readLine(line, static_cast<int>(rest)))
            {
                return false; // 時間切れ、またはエンジン終了
            }
            if (line == word)
                return true;
        }
        return false;
    }
    // prefix で始まる行が来るまで読み、その行を line に入れて true を返す
    bool waitForPrefix(const std::string &prefix, std::string &line, int timeoutMs)
    {
        auto deadline = Clock::now() + std::chrono::milliseconds(timeoutMs);
        while (true)
        {
            auto rest = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
            if (rest <= 0 || !readLine(line, static_cast<int>(rest)))
            {
                return false; // 時間切れ、またはエンジン終了
            }
            if (line.rfind(prefix, 0) == 0)
                return true;
        }
        return false;
    }
    void quit()
    {
        if (_pid == -1)
        {
            return;
        }
        send("quit");

        // 管を閉じる。エンジンには EOF (入力の終わり) も届くので、念押しになる
        close(_toEngine);
        close(_fromEngine);
        _toEngine = -1;
        _fromEngine = -1;
        _buffer.clear();

        // 終わったかを、10 ミリ秒ごとに最大 100 回 (約 1 秒) 確かめる
        // WNOHANG: 終わっていなくても待たずにすぐ返す (戻り値 0 = まだ動いている)
        int status;
        bool finished = false;
        for (int i = 0; i < 100; ++i)
        {
            if (waitpid(_pid, &status, WNOHANG) != 0)
            {
                // _pid が返った = 終わって後片付けも済んだ (-1 はエラーだが、待っても無駄なので同じ扱い)
                finished = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // 1 秒たっても終わらなければ強制終了し、止まるのを待って後片付けする
        if (!finished)
        {
            std::cerr << "エンジンが終了しないため強制終了します (pid " << _pid << ")" << std::endl;
            kill(_pid, SIGKILL);
            waitpid(_pid, &status, 0);
        }

        _pid = -1; // 2 回目の quit() では何もしないように
    }

    EngineProcess() = default;

    // 変数がなくなるとき (main の途中で return したときなど) に、自動でエンジンを止める
    ~EngineProcess() { quit(); }

    // コピー禁止: _pid が同じオブジェクトが 2 つできると、両方が同じエンジンを止めようとするため
    EngineProcess(const EngineProcess &) = delete;
    EngineProcess &operator=(const EngineProcess &) = delete;
};

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
    result.winner = Winner::Draw;

    // 盤面は対局プログラム自身も持つ (エンジンの手が合法かを確かめるため)
    Position p;
    if (!p.setSFEN(startSfen))
    {
        result.reason = "開始局面が不正";
        return result;
    }

    // 対局の準備: isready → readyok で、前の対局の読み残し (時間切れの後に
    // 遅れて届いた bestmove など) を読み捨ててから、新しい対局を始める
    for (EngineProcess *e : {&black, &white})
    {
        e->send("isready");
        if (!e->waitFor("readyok", 10000))
        {
            result.winner = (e == &black) ? Winner::White : Winner::Black;
            result.reason = "readyok が来ない";
            return result;
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
        black.send(winner == Winner::Draw    ? "gameover draw"
                   : winner == Winner::Black ? "gameover win"
                                             : "gameover lose");
        white.send(winner == Winner::Draw    ? "gameover draw"
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
            // 入玉宣言が正しいかの判定はまだ作っていないので、引き分け扱いにしておく
            return finish(Winner::Draw, "入玉宣言 (判定は未対応)");
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
    return w == Winner::Black ? "先手の勝ち" : w == Winner::White ? "後手の勝ち"
                                                                   : "引き分け";
}

int main(int argc, char *argv[])
{
    signal(SIGPIPE, SIG_IGN);
    // 初期化
    Shogi::initialize();
    // 局面のインスタンスを作成
    Position p;
    p.init();
    if (argc != 3)
    {
        std::cout << "エンジンパスを２つ指定してください。" << std::endl;
        return 0;
    }
    std::string engine1_path = argv[1];
    std::string engine2_path = argv[2];
    EngineProcess engine1;
    EngineProcess engine2;
    if (!engine1.start(engine1_path) || !engine2.start(engine2_path))
    {
        return 1;
    }
    for (EngineProcess *e : {&engine1, &engine2})
    {
        e->send("usi");
        if (!e->waitFor("usiok", 3000))
        {
            std::cerr << "usiok が来ない" << std::endl;
            return 1;
        }
        e->send("setoption name Depth value 3");
    }
    std::string initial_sfen = random_play(p);
    std::cout << "開始局面: " << initial_sfen << std::endl;

    GameResult r = play_game(engine1, engine2, initial_sfen, 5000);
    std::cout << winnerName(r.winner) << " (" << r.reason << ")" << std::endl;
    std::cout << "指し手: " << r.moves << std::endl;

    engine1.quit();
    engine2.quit();
    return 0;
}