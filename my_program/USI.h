#ifndef GAME_USI_H
#define GAME_USI_H

#include <chrono>
#include <string>
#include <Shogi.h>
#include <vector>

class usiConnection
{
    static const std::string ID;
    static const std::string AUTHOR;

public:
    // function for USI protocol
    void usi();
    void usinewgame();
    void setoption();
    void isready();
    void position();
    void set_limits();
    void go();
    void stop();
    void ponderhit();
    void quit();

    // utilityfunctions
    // USIプロトコルをコマンドごとに分割してstd::vectorに格納する
    std::vector<std::string> split(std::string s);
};

#endif