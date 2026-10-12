#include "USI.h"
#include <Shogi.h>
#include <vector>
#include <iostream>

int main()
{
    std::vector<std::string> s;
    usiConnection usi;
    // s = usi.split("aaa bbb ccc");
    // for (size_t i = 0; i < s.size(); i++)
    // {
    //     std::cout << s[i] << "\n";
    // }
    usi.run();

    return 0;
}