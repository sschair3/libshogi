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
    // USI protocol commands

public:
    static const std::string USI_COMMAND;
    static const std::string USINEWGAME_COMMAND;
    static const std::string SETOPTION_COMMAND;
    static const std::string ISREADY_COMMAND;
    static const std::string POSITION_COMMAND;
    static const std::string SET_LIMITS_COMMAND;
    static const std::string GO_COMMAND;
    static const std::string STOP_COMMAND;
    static const std::string PONDERHIT_COMMAND;
    static const std::string QUIT_COMMAND;
    // function for USI protocol
    void usi();
    void usinewgame();
    void setoption(std::string command);
    void isready();
    game::Position position(std::string command, game::Position &p);
    void set_limits();
    void go();
    void stop();
    void ponderhit();
    void gameover();
    void quit();

    // utilityfunctions
    // USIプロトコルをコマンドごとに分割してstd::vectorに格納する
    std::vector<std::string> split(std::string s);
};

#endif