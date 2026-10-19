#include <iostream>
#include <fstream>
#include <string>
#include <vector>

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
        if (lines[lines.size() - i] == "'summary:abnormal")
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
            shohai = lines[toryo_line - i][0] == '+' ? 1 : -1;
            break;
        }
        else
            i++;
    }
    return shohai;
}

int main()
{
    std::string file_path = "../../2026/01/01/wdoor+floodgate-300-10F+910+akeome2026+20260101050006.csa";
    std::cout << judge(file_path) << std::endl;
    return 0;
}
