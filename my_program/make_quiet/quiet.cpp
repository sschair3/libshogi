/**
 * quiet.cpp
 *
 * 棋譜を再生して、「落ち着いた局面」だけの特徴量を書き出す (学習用)。
 * 書き出す形式は test.cpp と同じ (対局ID ラベル 手数 特徴量...) なので、
 * convert_np.py / train.py / train_kp.py をそのまま使える。
 *
 * 落ち着いた局面 = 次の 2 つを両方満たす局面
 *   1. 手番側が王手されていない
 *   2. 駒を取り合っても、手番側が駒得できない
 *      (駒割りだけで評価した静止探索の値が、何もしない場合の駒割りと同じ)
 *
 * 駒割りには engine1 と同じ駒の価値 (Evaluation.h の Value / Hands) を使う。
 * 学習した評価関数を使うと、ゆがんだ駒の価値で局面を選ぶことになるため。
 *
 * 使い方 (make_quiet フォルダで):
 *   make && ./quiet [棋譜のフォルダ] [書き出し先のフォルダ]
 *   既定: ../../../2026 → ../../../learn/data_quiet  (01.txt 〜 10.txt を作る)
 */

#include <algorithm>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <vector>

#include <Array.h>
#include <CSAFile.h>
#include <Shogi.h>

using namespace game;
using namespace Evaluation;

using MoveList = foundation::Array<Move::Move, Move::Max>;

// 特徴量の番号 (test.cpp と同じ。書き出すときは +1 して、符号で先手 / 後手を表す)
static const int HandBase = 16 * 81;        // 1296
static const int TurnIndex = HandBase + 16; // 1312

/// 静止探索で読む最大の手数 (駒の取り合いが長く続く局面で時間がかからないように)
static const int QuiesMaxPly = 8;

/* ------------------------------------------------------------ 棋譜の読み込み (test.cpp と同じ) */

/// 勝敗判定: 先手勝ち = 1、後手勝ち = -1、使わない棋譜 = 0 (投了で終わった棋譜だけ使う)
static int judge(const std::string &kifu_path)
{
    std::ifstream dataFile(kifu_path);
    if (!dataFile)
        return 0;
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(dataFile, line))
        lines.push_back(line);
    if (lines.size() < 50)
        return 0;
    for (size_t i = 1; i < 10; ++i)
    {
        // 'summary:abnormal:... と続くので先頭一致で判定する
        if (lines[lines.size() - i].rfind("'summary:abnormal", 0) == 0)
            return 0;
    }
    int toryo_line = -1;
    for (size_t i = 0; i < lines.size(); i++)
    {
        if (!lines[i].empty() && lines[i][0] == '%')
        {
            toryo_line = static_cast<int>(i);
            break;
        }
    }
    if (toryo_line == -1 || lines[toryo_line] != "%TORYO")
        return 0;
    // %TORYO の前で最後に指した側が勝ち
    for (int i = toryo_line - 1; i >= 0; --i)
    {
        if (!lines[i].empty() && (lines[i][0] == '+' || lines[i][0] == '-'))
            return lines[i][0] == '+' ? 1 : -1;
    }
    return 0;
}

/// ディレクトリ内の名前を並べて返す ("." で始まるものは除く)
static std::vector<std::string> listDir(const std::string &dir)
{
    std::vector<std::string> names;
    DIR *d = opendir(dir.c_str());
    if (d == nullptr)
        return names;
    while (dirent *e = readdir(d))
    {
        if (e->d_name[0] != '.')
            names.push_back(e->d_name);
    }
    closedir(d);
    std::sort(names.begin(), names.end());
    return names;
}

/// 1局面ぶんの特徴量を書き出す (test.cpp と同じ形式)
static void writeFeatures(std::ostream &out, const Position &p)
{
    for (auto sq : Square::all)
    {
        Piece::Piece pc = p.square(sq);
        if (pc == Piece::EMP)
            continue;
        int kind = static_cast<int>(pc & Piece::Neutral);
        if ((pc & Piece::Color) == Piece::White)
            out << ' ' << -(kind * 81 + (80 - sq) + 1);
        else
            out << ' ' << +(kind * 81 + sq + 1);
    }
    for (Piece::Piece kind = Piece::FU; kind <= Piece::KI; kind++)
    {
        for (int n = 0; n < p.hand(Color::Black, kind); n++)
            out << ' ' << +(HandBase + static_cast<int>(kind) + 1);
        for (int n = 0; n < p.hand(Color::White, kind); n++)
            out << ' ' << -(HandBase + static_cast<int>(kind) + 1);
    }
    out << ' ' << (p.turn() == Color::Black ? TurnIndex + 1 : -(TurnIndex + 1));
}

/* ------------------------------------------------------------ 落ち着いているかの判定 */

/// 駒割り (手番側から見た値)。p.eval() は先手から見た駒割りを差分で持っている
static Eval material(Position &p)
{
    Eval e = p.eval();
    return (p.turn() == Color::Black) ? e : -e;
}

/**
 * 駒割りだけで評価する静止探索 (ネガマックス、手番側から見た値)
 *   王手されていなければ、駒を取る手だけを読む (何もしない = stand pat も選べる)
 *   王手されていれば、逃げる手を全部読む
 */
static Eval quiescence(Position &p, Eval alpha, Eval beta, int ply)
{
    if (ply >= QuiesMaxPly)
        return material(p);

    MoveList m;
    const bool inCheck = (p.nchecks() > 0);
    if (inCheck)
    {
        p.genMove(m);
        if (m.vsize() == 0)
            return -Infinity + ply; // 詰み
    }
    else
    {
        Eval standPat = material(p);
        if (standPat >= beta)
            return beta;
        if (standPat > alpha)
            alpha = standPat;
        p.genCapt(m);
    }

    for (auto move : m)
    {
        auto back = p.move(move);
        if (!inCheck && !p.lastMoveIsLegal())
        {
            p.undo(back); // genCapt() が作る非合法手 (自玉を取られる手)
            continue;
        }
        Eval value = -quiescence(p, -beta, -alpha, ply + 1);
        p.undo(back);
        if (value >= beta)
            return beta;
        if (value > alpha)
            alpha = value;
    }
    return alpha;
}

/// 局面の分類
enum class Kind
{
    Quiet,   // 落ち着いている (書き出す)
    InCheck, // 王手されている
    Tactical // 駒を取り合うと手番側が駒得できる
};

static Kind classify(Position &p)
{
    if (p.nchecks() > 0)
        return Kind::InCheck;
    // 窓 [standPat, standPat + 1] で探索する: standPat より良くなる手があるかだけを調べる
    Eval standPat = material(p);
    Eval v = quiescence(p, standPat, standPat + 1, 0);
    return (v > standPat) ? Kind::Tactical : Kind::Quiet;
}

/* ------------------------------------------------------------ main */

int main(int argc, char *argv[])
{
    const std::string root = (argc >= 2) ? argv[1] : "../../../2026";
    const std::string outDir = (argc >= 3) ? argv[2] : "../../../learn/data_quiet";
    mkdir(outDir.c_str(), 0755);

    Shogi::initialize();
    Position::setValue(Value);   // engine1 と同じ駒の価値
    Position::handsValue(Hands);

    long allTotal = 0, allQuiet = 0, allCheck = 0, allTactical = 0;

    for (int month = 1; month <= 10; ++month)
    {
        char mm[3];
        std::snprintf(mm, sizeof mm, "%02d", month);
        std::ofstream out(outDir + "/" + mm + ".txt");
        if (!out)
        {
            std::cerr << "書き出せません: " << outDir << "/" << mm << ".txt" << std::endl;
            return 1;
        }

        int gameId = 0;
        long total = 0, quiet = 0, check = 0, tactical = 0;
        for (const auto &day : listDir(root + "/" + mm))
        {
            for (const auto &name : listDir(root + "/" + mm + "/" + day))
            {
                std::string path = root + "/" + mm + "/" + day + "/" + name;
                int result = judge(path);
                if (result == 0)
                    continue;
                int label = (result == 1) ? 1 : 0; // 先手から見たラベル

                try
                {
                    CSAFile kifu(path);
                    Position p(kifu.summary());

                    // 1局ぶん溜めてから書く (途中で失敗したときに半端な対局を書かないため)
                    std::ostringstream buf;
                    long t = 0, q = 0, c = 0, a = 0;
                    int ply = 0;
                    auto visit = [&]()
                    {
                        ++t;
                        switch (classify(p))
                        {
                        case Kind::Quiet:
                            ++q;
                            buf << gameId << ' ' << label << ' ' << ply;
                            writeFeatures(buf, p);
                            buf << '\n';
                            break;
                        case Kind::InCheck:
                            ++c;
                            break;
                        case Kind::Tactical:
                            ++a;
                            break;
                        }
                    };
                    visit();
                    for (const auto &m : kifu)
                    {
                        p.move(m);
                        ++ply;
                        visit();
                    }
                    out << buf.str();
                    total += t, quiet += q, check += c, tactical += a;
                    ++gameId;
                }
                catch (...)
                {
                    // 読めない棋譜は飛ばす
                }
            }
        }

        std::cout << mm << ".txt: 対局 " << gameId << "  局面 " << total
                  << "  → 落ち着いた局面 " << quiet << " (王手 " << check << ", 駒得の手あり " << tactical << ")"
                  << std::endl;
        allTotal += total, allQuiet += quiet, allCheck += check, allTactical += tactical;
    }

    std::cout << "合計: 局面 " << allTotal << "  落ち着いた局面 " << allQuiet
              << " (" << (100.0 * static_cast<double>(allQuiet) / static_cast<double>(allTotal)) << "%)"
              << "  王手 " << allCheck << "  駒得の手あり " << allTactical << std::endl;
    return 0;
}
