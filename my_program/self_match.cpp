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

using namespace game;
using namespace foundation;
using namespace Evaluation;
using Clock = std::chrono::steady_clock;

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

std::string random_play()
{
    //------------------------------------------------------------/
    // ランダムに16手指した後のsfenを返す関数
    //-----------------------------------------------------------//
    // 初期化
    Shogi::initialize();
    // 局面のインスタンスを作成
    Position p;
    p.init();
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
    return p.sfen();
}

int main(int argc, char *argv[])
{
    signal(SIGPIPE, SIG_IGN);
    if (argc != 3)
    {
        std::cout << "エンジンパスを２つ指定してください。" << std::endl;
        return 0;
    }
    std::string engine1_path = argv[1];
    std::string engine2_path = argv[2];
    EngineProcess engine1;
    // EngineProcess engine2;
    engine1.start(engine1_path);
    // engine1.start("/private/tmp/claude-501/-Users-shogo-shogi/07402087-84d0-41b4-bb79-670bda3afa82/scratchpad/slow_engine.sh");
    // engine2.start(engine2_path);
    engine1.send("usi");
    engine1.quit();
    return 0;
}