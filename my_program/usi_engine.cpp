/**
 * usi_engine.cpp
 *
 * USI プロトコル対応の将棋エンジン本体。
 * ビルド: make FILE_NAME=usi_engine
 * GUI (ShogiHome など) にこの実行ファイルをエンジンとして登録して使う。
 */

#include "USI.h"

int main()
{
    usiConnection usi;
    usi.run();
    return 0;
}
