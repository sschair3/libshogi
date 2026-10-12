#include "USI.h"
#include <iostream>
#include <string>
#include <vector>
#include <format>
#include <Shogi.h>

const std::string usiConnection::ID = "my_shogi_lib";
const std::string usiConnection::AUTHOR = "伊藤匠吾";

const std::string usiConnection::USI_COMMAND = "usi";
const std::string usiConnection::USINEWGAME_COMMAND = "usinewgame";
const std::string usiConnection::SETOPTION_COMMAND = "setoption";
const std::string usiConnection::ISREADY_COMMAND = "isready";
const std::string usiConnection::POSITION_COMMAND = "position";
const std::string usiConnection::SET_LIMITS_COMMAND = "set_limits";
const std::string usiConnection::GO_COMMAND = "go";
const std::string usiConnection::STOP_COMMAND = "stop";
const std::string usiConnection::PONDERHIT_COMMAND = "ponderhit";
const std::string usiConnection::QUIT_COMMAND = "quit";

void usiConnection::usi()
{
    std::cout << "id name " << usiConnection::ID << std::endl;
    std::cout << "id author " << usiConnection::AUTHOR << std::endl;
    std::cout << "usiok" << std::endl;
};
void usiConnection::usinewgame()
{
    std::cout << "usinewgame" << std::endl;
};
void usiConnection::setoption(std::string command)
{
    std::vector<std::string> s = split(command);
};
void usiConnection::isready()
{
    game::Shogi::initialize();
    game::Position::setValue(game::Evaluation::Value);
    game::Position::handsValue(game::Evaluation::Hands);
    game::Position p;
    p.init();
    std::cout << "readyok" << std::endl;
};
game::Position usiConnection::position(std::string command, game::Position &current_p)
{
    std::vector<std::string> s = split(command);
    if (s.back() == "startpos")
    {
        return current_p;
    }
    current_p.move(s.back());
    return current_p;
};
void usiConnection::set_limits() {};
void usiConnection::go() {};
void usiConnection::stop() {};
void usiConnection::ponderhit() {};
void usiConnection::quit() {};
void usiConnection::gameover() {};
std::vector<std::string> usiConnection::split(std::string s)
{
    std::vector<std::string> ssplit;
    std::string tempS;
    for (size_t i = 0; i < s.size(); i++)
    {
        if (s[i] == ' ')
        {
            ssplit.push_back(tempS);
            tempS = "";
        }
        else if (s[i] != ' ')
        {
            tempS += s[i];
        }
    }
    ssplit.push_back(tempS);
    tempS = "";
    return ssplit;
}
