#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <dirent.h>

#include <Shogi.h>
#include <CSAFile.h>

using namespace game;

// 特徴量の番号 (書き出すときは +1 して、符号で先手 / 後手を表す)
//   盤上の駒 : kind * 81 + マス             (0 〜 1295)
//   持ち駒   : HandBase + kind  (枚数だけ繰り返す)
//   手番     : TurnIndex
static const int HandBase = 16 * 81;        // 1296
static const int TurnIndex = HandBase + 16; // 1312

// 勝敗判定
int judge(std::string kifu_path)
{
    int shohai = 0;
    std::fstream dataFile(kifu_path, std::ios::in);
    if (!dataFile.is_open())
    {
        std::cerr << "ファイルを開けませんでした。" << std::endl;
        return 0;
    }
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(dataFile, line))
        lines.push_back(line);
    if (lines.size() < 50)
    {
        std::cerr << "csaファイルが短すぎるため切り捨てます。" << std::endl;
        return 0;
    }
    int i = 1;
    while (i < 10)
    {
        // 'summary:abnormal:... と続くので先頭一致で判定する
        if (lines[lines.size() - i].rfind("'summary:abnormal", 0) == 0)
        {

            std::cerr << "異常終了した棋譜のため切り捨てます。" << std::endl;
            return 0;
        }
        else
            i++;
    }

    int toryo_line = -1;
    for (size_t i = 0; i < lines.size(); i++)
    {
        if (lines[i][0] == '%')
        {
            toryo_line = i;
            break;
        }
    }
    if (toryo_line == -1)
    {
        std::cerr << "終了コマンドが見つかりませんでした。" << std::endl;
        return 0;
    }
    else if (lines[toryo_line] != "%TORYO")
    {
        std::cerr << "投了による負けではないのでここでプログラムを終了します。" << std::endl;
        return 0;
    }
    i = 1;
    while (true)
    {
        if (lines[toryo_line - i][0] == '+' || lines[toryo_line - i][0] == '-')
        {
            if (lines[toryo_line - i][0] == '+')
            {
                std::cerr << "先手勝ち" << std::endl;
                shohai = 1;
            }
            else
            {
                std::cerr << "後手勝ち" << std::endl;
                shohai = -1;
            }

            break;
        }
        else
            i++;
    }
    return shohai;
}

// ディレクトリ内の名前を並べて返す ("." で始まるものは除く)
std::vector<std::string> listDir(const std::string &dir)
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

// 1局面ぶんの特徴量を書き出す (先手から見た向き)
void writeFeatures(std::ostream &out, const Position &p)
{
    // 盤上の駒
    for (auto sq : Square::all)
    {
        Piece::Piece pc = p.square(sq);
        if (pc == Piece::EMP)
            continue;
        int kind = pc & Piece::Neutral;
        if ((pc & Piece::Color) == Piece::White)
            out << ' ' << -(kind * 81 + (80 - sq) + 1);
        else
            out << ' ' << +(kind * 81 + sq + 1);
    }

    // 持ち駒 (枚数だけ同じ番号を繰り返す)
    for (Piece::Piece kind = Piece::FU; kind <= Piece::KI; kind++)
    {
        for (int n = 0; n < p.hand(Color::Black, kind); n++)
            out << ' ' << +(HandBase + (int)kind + 1);
        for (int n = 0; n < p.hand(Color::White, kind); n++)
            out << ' ' << -(HandBase + (int)kind + 1);
    }

    // 手番
    out << ' ' << (p.turn() == Color::Black ? TurnIndex + 1 : -(TurnIndex + 1));
}

int main()
{
    Shogi::initialize();

    const std::string root = "../../2026/10";
    std::ofstream out("10.txt");

    int gameId = 0;
    int skipped = 0;
    long positions = 0;

    for (const auto &day : listDir(root))
    {
        for (const auto &name : listDir(root + "/" + day))
        {
            std::string path = root + "/" + day + "/" + name;

            int result = judge(path);
            if (result == 0)
            {
                skipped++;
                continue;
            }
            // 先手から見たラベル (先手勝ち = 1, 後手勝ち = 0)
            int label = result == 1 ? 1 : 0;

            try
            {
                CSAFile kifu(path);
                Position p(kifu.summary());

                // 途中で失敗したときに半端な対局を書かないよう、1局ぶん溜めてから書く
                std::ostringstream buf;
                long n = 0;

                int ply = 0;
                buf << gameId << ' ' << label << ' ' << ply;
                writeFeatures(buf, p);
                buf << '\n';
                n++;

                for (const auto &m : kifu)
                {
                    p.move(m);
                    ply++;
                    buf << gameId << ' ' << label << ' ' << ply;
                    writeFeatures(buf, p);
                    buf << '\n';
                    n++;
                }
                out << buf.str();
                positions += n;
                gameId++;
            }
            catch (...)
            {
                std::cerr << "棋譜を読めませんでした: " << path << std::endl;
                skipped++;
            }
        }
    }

    std::cerr << "対局数: " << gameId << "  局面数: " << positions
              << "  捨てた棋譜: " << skipped << std::endl;
    return 0;
}
