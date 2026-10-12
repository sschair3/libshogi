#ifndef GAME_USI_H
#define GAME_USI_H

#include <chrono>
#include <string>
#include <Shogi.h>
#include <vector>

class usiConnection
{
public:
    std::string ID = "my_shogi_lib";
    // void usiConnection::usi();
    // void usiConnection::usinewgame();
    // void usiConnection::setoption();
    // void usiConnection::isready();
    // void usiConnection::position();
    // void usiConnection::set_limits();
    // void usiConnection::go();
    // void usiConnection::stop();
    // void usiConnection::ponderhit();
    // void usiConnection::quit();
    void run();
    std::vector<std::string> split(std::string s);
};

#endif