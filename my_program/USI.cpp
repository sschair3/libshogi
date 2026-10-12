#include "USI.h"
#include <iostream>
#include <string>
#include <vector>

void usi() {

};
void usinewgame();
void setoption();
void isready();
void position();
void set_limits();
void go();
void stop();
void ponderhit();
void quit();

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
    return ssplit;
}

void usiConnection::run()
{
    std::string usi_prompt;
    std::getline(std::cin, usi_prompt);
    for (size_t i = 0; i < split(usi_prompt).size(); i++)
    {
        std::cout << split(usi_prompt)[i] << "\n";
    }
}