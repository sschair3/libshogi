#include "USI.h"
#include <Shogi.h>
#include <vector>
#include <iostream>

int main()
{
    std::vector<std::string> s;
    usiConnection usi;
    std::string usi_prompt;
    while (true)
    {
        std::getline(std::cin, usi_prompt);
        std::vector<std::string> command = usi.split(usi_prompt);
        if (command[0] == "quit")
        {
            std::cerr << "quit command received." << std::endl;
            std::cerr << "Quitting USI connection..." << std::endl;
            usi.quit();
            break;
        }
        else if (command[0] == "usi")
        {
            std::cerr << "usi command received." << std::endl;
            usi.usi();
        }
        else if (command[0] == "usinewgame")
        {
            std::cerr << "usinewgame command received." << std::endl;
            usi.usinewgame();
        }
        else if (command[0] == "setoption")
        {
            std::cerr << "setoption command received." << std::endl;
            usi.setoption();
        }
        else if (command[0] == "isready")
        {
            std::cerr << "isready command received." << std::endl;
            usi.isready();
        }
        else if (command[0] == "position")
        {
            std::cerr << "position command received." << std::endl;
            usi.position();
        }
        else if (command[0] == "set_limits")
        {
            std::cerr << "set_limits command received." << std::endl;
            usi.set_limits();
        }
        else if (command[0] == "go")
        {
            std::cerr << "go command received." << std::endl;
            usi.go();
        }
        else if (command[0] == "stop")
        {
            std::cerr << "stop command received." << std::endl;
            usi.stop();
        }
        else if (command[0] == "ponderhit")
        {
            std::cerr << "ponderhit command received." << std::endl;
            usi.ponderhit();
        }
    }

    return 0;
}