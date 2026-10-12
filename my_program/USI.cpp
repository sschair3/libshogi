#include "USI.h"
#include <iostream>
#include <string>
#include <vector>
#include <format>

const std::string usiConnection::ID = "my_shogi_lib";
const std::string usiConnection::AUTHOR = "伊藤匠吾";

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
void usiConnection::setoption() {};
void usiConnection::isready() {};
void usiConnection::position() {};
void usiConnection::set_limits() {};
void usiConnection::go() {};
void usiConnection::stop() {};
void usiConnection::ponderhit() {};
void usiConnection::quit() {};

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
