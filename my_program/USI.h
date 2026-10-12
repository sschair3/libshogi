#ifndef GAME_USI_H
#define GAME_USI_H

#include <string>
#include <vector>
#include <Shogi.h>

/**
 * USI プロトコルでGUIとやり取りするクラス
 *
 * 使い方: usiConnection usi; usi.run();
 * 標準入力から1行ずつコマンドを読み、標準出力に応答する。
 * デバッグ用の出力は必ず std::cerr に出すこと (std::cout は GUI が読む)。
 */
class usiConnection
{
public:
    // USI protocol commands (GUI -> エンジン)
    static const std::string USI_COMMAND;
    static const std::string USINEWGAME_COMMAND;
    static const std::string SETOPTION_COMMAND;
    static const std::string ISREADY_COMMAND;
    static const std::string POSITION_COMMAND;
    static const std::string GO_COMMAND;
    static const std::string STOP_COMMAND;
    static const std::string PONDERHIT_COMMAND;
    static const std::string GAMEOVER_COMMAND;
    static const std::string QUIT_COMMAND;

    /// コマンドを読んで処理するループ (quit が来るか入力が終わるまで)
    void run();

    // function for USI protocol (引数はコマンドを単語に分けたもの)
    void usi();
    void usinewgame();
    void setoption(const std::vector<std::string> &args);
    void isready();
    void position(const std::vector<std::string> &args);
    void go(const std::vector<std::string> &args);
    void stop();
    void ponderhit();
    void gameover(const std::vector<std::string> &args);
    void quit();

    // utility functions
    /// USIプロトコルのコマンドを空白で分割してstd::vectorに格納する
    static std::vector<std::string> split(const std::string &s);
    /// libshogi の指し手を USI 形式の文字列 (7g7f, 8h2b+, P*5e) に変換する
    static std::string moveToUSI(const game::Position &p, game::Move::Move m);
    /// USI 形式の文字列を、局面 p の合法手から探して libshogi の指し手にする
    static bool usiToMove(game::Position &p, const std::string &usi, game::Move::Move &m);

private:
    static const std::string ID;
    static const std::string AUTHOR;

    /// 現在の局面
    game::Position _pos;
    /// ライブラリを初期化済みか
    bool _initialized = false;
    /// 最大探索深さ (USI オプション "Depth" で変更できる)
    int _maxDepth = 5;
    /// go infinite のとき、stop が来るまで bestmove を出さずに保持しておく
    std::string _pendingBestmove;
    /// quit が来たか
    bool _quit = false;

    void initialize();
};

#endif
