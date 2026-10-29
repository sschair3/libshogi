#include <cstdlib>
#include <Usi.h>
#include "Search.h"
#include "eval.h"
#include <Array.h>
#include <bitset>

using MoveList = foundation::Array<game::Move::Move, game::Move::Max>;

int main()
{
    game::Shogi::initialize();
    game::Position p;
    p.init();
    MoveList move;
    p.genMove(move);
    for (auto movei : move)
    {
        std::cout << game::Usi::toUSI(movei)
                  << "  from=" << game::Move::from(movei)
                  << " to=" << game::Move::to(movei)
                  << "  " << std::bitset<16>(movei)
                  << std::endl;
    }
}
