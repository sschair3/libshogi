#ifndef _MY_ENGINE_EVALUATION_H
#define _MY_ENGINE_EVALUATION_H

#include "Evaluation.h"
#include "Position.h"
using namespace game;
using namespace Evaluation;

// 駒の位置の評価
static const Eval PST[16][81];
static Eval rocateEvaluation(game::Position &p);

#endif