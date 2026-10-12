#include "USI.h"
#include "Search.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <Shogi.h>
#include <Array.h>

using namespace game;

const std::string usiConnection::ID     = "my_shogi_lib";
const std::string usiConnection::AUTHOR = "伊藤匠吾";

const std::string usiConnection::USI_COMMAND        = "usi";
const std::string usiConnection::USINEWGAME_COMMAND = "usinewgame";
const std::string usiConnection::SETOPTION_COMMAND  = "setoption";
const std::string usiConnection::ISREADY_COMMAND    = "isready";
const std::string usiConnection::POSITION_COMMAND   = "position";
const std::string usiConnection::GO_COMMAND         = "go";
const std::string usiConnection::STOP_COMMAND       = "stop";
const std::string usiConnection::PONDERHIT_COMMAND  = "ponderhit";
const std::string usiConnection::GAMEOVER_COMMAND   = "gameover";
const std::string usiConnection::QUIT_COMMAND       = "quit";

/// 平手初期局面の SFEN
static const std::string StartSFEN =
    "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1";

/* ------------------------------ メインループ ------------------------------ */

void usiConnection::run()
{
    std::string line;
    // 入力が終わる (GUI が閉じる) か quit が来たら終了
    while (!_quit && std::getline(std::cin, line)) {

        std::vector<std::string> command = split(line);
        if (command.empty()) {
            continue;       // 空行は無視
        }
        std::cerr << "[usi] < " << line << std::endl;

        const std::string &cmd = command[0];
        if      (cmd == USI_COMMAND)        usi();
        else if (cmd == ISREADY_COMMAND)    isready();
        else if (cmd == SETOPTION_COMMAND)  setoption(command);
        else if (cmd == USINEWGAME_COMMAND) usinewgame();
        else if (cmd == POSITION_COMMAND)   position(command);
        else if (cmd == GO_COMMAND)         go(command);
        else if (cmd == STOP_COMMAND)       stop();
        else if (cmd == PONDERHIT_COMMAND)  ponderhit();
        else if (cmd == GAMEOVER_COMMAND)   gameover(command);
        else if (cmd == QUIT_COMMAND)       quit();
        else {
            // 知らないコマンドは無視する (USI の決まり)
            std::cerr << "[usi] unknown command: " << cmd << std::endl;
        }
    }
}

/* ------------------------------ 各コマンド ------------------------------ */

void usiConnection::usi()
{
    std::cout << "id name " << ID << std::endl;
    std::cout << "id author " << AUTHOR << std::endl;
    // GUI に設定項目を知らせる (GUI のエンジン設定画面に表示される)
    std::cout << "option name Depth type spin default " << _maxDepth
              << " min 1 max 10" << std::endl;
    std::cout << "usiok" << std::endl;
}

void usiConnection::initialize()
{
    if (_initialized) {
        return;
    }
    Shogi::initialize();
    Position::setValue(Evaluation::Value);
    Position::handsValue(Evaluation::Hands);
    _pos.init();
    _initialized = true;
}

void usiConnection::isready()
{
    // 重い初期化はここで済ませる
    initialize();
    std::cout << "readyok" << std::endl;
}

// setoption name <名前> value <値>
void usiConnection::setoption(const std::vector<std::string> &args)
{
    std::string name, value;
    for (size_t i = 1; i + 1 < args.size(); ++i) {
        if (args[i] == "name")  name  = args[i + 1];
        if (args[i] == "value") value = args[i + 1];
    }
    if (name == "Depth" && !value.empty()) {
        int d = std::atoi(value.c_str());
        if (d >= 1) {
            _maxDepth = d;
        }
    }
    // USI_Hash や USI_Ponder など、対応していないオプションは無視する
}

void usiConnection::usinewgame()
{
    // 対局ごとの初期化 (今は特になし)。usinewgame には応答しない。
    initialize();
}

// position startpos [moves m1 m2 ...]
// position sfen <盤面> <手番> <持ち駒> <手数> [moves m1 m2 ...]
void usiConnection::position(const std::vector<std::string> &args)
{
    initialize();

    if (args.size() < 2) {
        return;
    }

    size_t i = 1;
    if (args[i] == "startpos") {
        ++i;
    } else if (args[i] == "sfen") {
        // TODO: 任意の SFEN の読み込み。今は平手初期局面の SFEN だけ受け付ける
        std::string sfen;
        for (++i; i < args.size() && args[i] != "moves"; ++i) {
            sfen += (sfen.empty() ? "" : " ") + args[i];
        }
        if (sfen != StartSFEN) {
            std::cout << "info string sfen position is not supported yet" << std::endl;
        }
    } else {
        return;
    }

    // 毎回、初期局面から指し手を全部並べ直す
    _pos.init();

    if (i < args.size() && args[i] == "moves") {
        for (++i; i < args.size(); ++i) {
            Move::Move m;
            if (!usiToMove(_pos, args[i], m)) {
                std::cout << "info string illegal move: " << args[i] << std::endl;
                std::cerr << "[usi] illegal move: " << args[i] << std::endl;
                break;
            }
            _pos.move(m);
        }
    }
}

// go [btime x] [wtime x] [byoyomi x] [binc x] [winc x] [infinite]
void usiConnection::go(const std::vector<std::string> &args)
{
    initialize();

    long btime = 0, wtime = 0, byoyomi = 0, binc = 0, winc = 0;
    bool infinite = false;
    for (size_t i = 1; i < args.size(); ++i) {
        auto next = [&]() { return (i + 1 < args.size()) ? std::atol(args[++i].c_str()) : 0L; };
        if      (args[i] == "btime")    btime   = next();
        else if (args[i] == "wtime")    wtime   = next();
        else if (args[i] == "byoyomi")  byoyomi = next();
        else if (args[i] == "binc")     binc    = next();
        else if (args[i] == "winc")     winc    = next();
        else if (args[i] == "infinite") infinite = true;
    }

    // 思考時間の目安 (秒)。秒読みがあればその 8 割、なければ残り時間の 1/40 + 加算
    bool blackToMove = (_pos.turn() == Color::Black);
    long remain = blackToMove ? btime : wtime;
    long inc    = blackToMove ? binc  : winc;
    double timeLimit = 0.0;
    if (!infinite) {
        if (byoyomi > 0) {
            timeLimit = static_cast<double>(byoyomi) / 1000.0 * 0.8;
        } else if (remain > 0 || inc > 0) {
            timeLimit = (static_cast<double>(remain) / 40.0 + static_cast<double>(inc)) / 1000.0;
        }
    }

    // 1 つの深さを読み終えるたびに info を出す
    auto printInfo = [&](const search::Result &r) {
        // USI の評価値は「手番側から見た値」なので、後手番なら符号を反転
        int v = blackToMove ? r.value : -r.value;
        std::cout << "info depth " << r.depth;
        if (v >= search::MateThreshold) {
            std::cout << " score mate " << (Evaluation::Infinity - v);
        } else if (v <= -search::MateThreshold) {
            std::cout << " score mate -" << (Evaluation::Infinity + v);
        } else {
            std::cout << " score cp " << v;
        }
        std::cout << " nodes " << r.nodes
                  << " time "  << static_cast<long>(r.seconds * 1000)
                  << " pv "    << moveToUSI(_pos, r.best) << std::endl;
    };

    search::Result r = search::think(_pos, _maxDepth, timeLimit, printInfo);

    std::string bestmove = r.hasMove ? moveToUSI(_pos, r.best) : "resign";

    if (infinite) {
        // go infinite のときは stop が来てから bestmove を返す決まり
        _pendingBestmove = bestmove;
    } else {
        std::cout << "bestmove " << bestmove << std::endl;
    }
}

void usiConnection::stop()
{
    // 探索は go の中で終わっているので、保留中の bestmove があれば返すだけ
    // TODO: 探索を別スレッドにして、探索途中で止められるようにする
    if (!_pendingBestmove.empty()) {
        std::cout << "bestmove " << _pendingBestmove << std::endl;
        _pendingBestmove.clear();
    }
}

void usiConnection::ponderhit()
{
    // TODO: 先読み (ponder) は未対応。USI_Ponder オプションを出していないので通常は来ない
}

void usiConnection::gameover(const std::vector<std::string> &)
{
    // gameover win / lose / draw。今は何もしない
    _pendingBestmove.clear();
}

void usiConnection::quit()
{
    _quit = true;
}

/* ------------------------------ ユーティリティ ------------------------------ */

std::vector<std::string> usiConnection::split(const std::string &s)
{
    std::vector<std::string> words;
    std::istringstream is(s);
    std::string w;
    while (is >> w) {
        words.push_back(w);
    }
    return words;
}

// CSA 形式 (+7776FU, +0055FU) を経由して USI 形式 (7g7f, P*5e) に変換する
std::string usiConnection::moveToUSI(const Position &p, Move::Move m)
{
    std::string csa = p.string(m);              // 例: "+7776FU"
    std::string usi;

    if (csa[1] == '0' && csa[2] == '0') {
        // 駒打ち: 駒の種類 + '*' + 打つ場所
        std::string pc = csa.substr(5, 2);
        const char *names[][2] = {
            {"FU", "P"}, {"KY", "L"}, {"KE", "N"}, {"GI", "S"},
            {"KI", "G"}, {"KA", "B"}, {"HI", "R"}
        };
        for (auto &n : names) {
            if (pc == n[0]) {
                usi += n[1];
            }
        }
        usi += '*';
    } else {
        // 移動元: 筋はそのまま、段 1..9 を a..i に
        usi += csa[1];
        usi += static_cast<char>('a' + (csa[2] - '1'));
    }
    // 移動先
    usi += csa[3];
    usi += static_cast<char>('a' + (csa[4] - '1'));

    // 成り
    if (m & Move::Promote) {
        usi += '+';
    }
    return usi;
}

// 合法手を全部 USI 形式にして、一致するものを探す
bool usiConnection::usiToMove(Position &p, const std::string &usi, Move::Move &m)
{
    foundation::Array<Move::Move, Move::Max> moves;
    p.genMove(moves);
    for (auto mv : moves) {
        if (moveToUSI(p, mv) == usi) {
            m = mv;
            return true;
        }
    }

    // libshogi の genMove は、歩・角・飛の「成れるのに成らない手」を生成しない。
    // 相手がその手を指した場合は、成る手から成りのビットを外して作る。
    if (!usi.empty() && usi.back() != '+') {
        for (auto mv : moves) {
            if ((mv & Move::Promote) && moveToUSI(p, mv) == usi + "+") {
                m = static_cast<Move::Move>(mv & ~static_cast<uint32_t>(Move::Promote));
                return true;
            }
        }
    }
    return false;
}
