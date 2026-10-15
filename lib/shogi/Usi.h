/**
 *****************************************************************************

 @file       Usi.h

 @brief      USI protocol and SFEN support

  libshogi に USI プロトコル / SFEN 形式の対応を追加するファイル。

   - 指し手の変換       : Usi::toUSI() / Usi::parseUSI() / Usi::fromUSI()
   - 局面の変換 (SFEN)  : Position::setSFEN() / Position::sfen()
                          (Position のメンバ関数だが、実装は Usi.cpp にある)
   - プロトコル処理     : USIConnection クラス
                          探索部分 think() だけをエンジン側で実装する

  This file is part of libshogi (modified version).
  libshogi is distributed under the GNU Lesser General Public License v3.

 *****************************************************************************/

#ifndef _GAME_USI_H
#define _GAME_USI_H

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include <Shogi.h>

// begin namespace 'game'
namespace game {

/* ------------------------------ 指し手の変換 ------------------------------ */

// begin namespace 'game::Usi'
namespace Usi {

/// 平手初期局面の SFEN
extern const std::string StartSFEN;

/// 詰みの評価値の目安。|評価値| >= MateThreshold なら
/// 「Infinity - 評価値の絶対値」手で詰むとみなして info に score mate を出す
const Evaluation::Eval   MateThreshold = Evaluation::Infinity - 1000;

/**
 * 指し手を USI 形式の文字列にする (7g7f, 8h2b+, P*5e)
 * 局面を見ずに変換できる。Move::None なら "none" を返す
 */
std::string toUSI (Move::Move m);

/**
 * USI 形式の文字列を指し手にする (書式だけを見て、合法かどうかは見ない)
 * @return 指し手。書式が正しくなければ Move::None
 */
Move::Move  parseUSI (const std::string &s);

/**
 * USI 形式の文字列を、局面 p で合法な指し手に変換する
 * libshogi の genMove() が生成しない「歩・角・飛などの不成」にも対応する
 * @param p 局面
 * @param s USI 形式の指し手
 * @param m 変換結果を受け取る
 * @return 合法手なら true
 */
bool        fromUSI (Position &p, const std::string &s, Move::Move &m);

/// go コマンドで指定される持ち時間など (時間はミリ秒)
struct Limits {
    long        btime    = 0;       ///< 先手の残り時間
    long        wtime    = 0;       ///< 後手の残り時間
    long        byoyomi  = 0;       ///< 秒読み
    long        binc     = 0;       ///< 先手の 1 手ごとの加算 (フィッシャー)
    long        winc     = 0;       ///< 後手の 1 手ごとの加算
    bool        infinite = false;   ///< go infinite (stop が来るまで考える)

    /**
     * 思考時間の目安 (秒) を簡単な方法で決める
     * 秒読みがあればその 8 割、なければ残り時間の 1/40 + 加算。
     * @return 秒。0 なら時間制限なし
     */
    double      thinkTime (Color::Color turn) const;
};

// end namespace 'game::Usi'
}

/* ---------------------------- USI プロトコル処理 ---------------------------- */

/**
 * USI プロトコルで GUI とやり取りするクラス
 *
 * 使い方: このクラスを継承して think() を実装し、run() を呼ぶ。
 *
 *   class MyEngine : public game::USIConnection {
 *   public:
 *       MyEngine() : USIConnection("名前", "作者") {}
 *   protected:
 *       game::Move::Move think(game::Position &p, const game::Usi::Limits &l) override;
 *   };
 *   int main() { MyEngine e; e.run(); }
 *
 * 標準出力 (std::cout) は GUI が読むので、デバッグ出力は std::cerr に出すこと。
 */
class USIConnection {

public:

    // USI protocol commands (GUI -> エンジン)
    static const std::string USI_COMMAND;
    static const std::string ISREADY_COMMAND;
    static const std::string SETOPTION_COMMAND;
    static const std::string USINEWGAME_COMMAND;
    static const std::string POSITION_COMMAND;
    static const std::string GO_COMMAND;
    static const std::string STOP_COMMAND;
    static const std::string PONDERHIT_COMMAND;
    static const std::string GAMEOVER_COMMAND;
    static const std::string QUIT_COMMAND;

    /// コンストラクタ (エンジン名と作者名)
    USIConnection (const std::string &name, const std::string &author);

    /// デストラクタ
    virtual ~USIConnection () {}

    /// コマンドを読んで処理するループ (quit が来るか入力が終わるまで)
    void                        run     (std::istream &in = std::cin);

    /// コマンドを空白で単語に分割する
    static std::vector<std::string>
                                split   (const std::string &s);

protected:

    /* ------------- エンジン側で実装 / 上書きする関数 ------------- */

    /**
     * 局面 p の最善手を返す (go コマンドで呼ばれる)
     * @return 最善手。Move::None を返すと投了 (bestmove resign)
     */
    virtual Move::Move          think     (Position &p, const Usi::Limits &limits) = 0;

    /// usi コマンドのとき、エンジンのオプションを "option name ..." の形で出力する
    virtual void                options   (std::ostream &) {}

    /// setoption name <name> value <value> を受け取ったとき
    virtual void                setOption (const std::string &, const std::string &) {}

    /// isready のとき (ライブラリの初期化後に呼ばれる。評価関数の読み込みなど)
    virtual void                ready     (void) {}

    /// usinewgame のとき
    virtual void                newGame   (void) {}

    /// gameover win / lose / draw のとき
    virtual void                gameOver  (const std::string &) {}

    /* ------------------ エンジン側から使う関数 ------------------ */

    /**
     * 探索の途中経過を info として出力する
     * @param p       局面 (手番を見て評価値の向きを変える)
     * @param depth   探索深さ
     * @param value   評価値 (先手から見た値)
     * @param nodes   探索ノード数
     * @param seconds 経過時間 (秒)
     * @param best    最善手
     */
    void                        info      (const Position &p, int depth,
                                           Evaluation::Eval value, uint64_t nodes,
                                           double seconds, Move::Move best) const;

private:

    // 各コマンドの処理
    void                        _usi       (void);
    void                        _isready   (void);
    void                        _setoption (const std::vector<std::string> &);
    void                        _usinewgame(void);
    void                        _position  (const std::vector<std::string> &);
    void                        _go        (const std::vector<std::string> &);
    void                        _stop      (void);
    void                        _gameover  (const std::vector<std::string> &);

    /// ライブラリの初期化 (1 回だけ)
    void                        _initialize(void);

    /// エンジン名・作者名
    std::string                 _name;
    std::string                 _author;

    /// 現在の局面
    Position                    _pos;

    /// 初期化済みか
    bool                        _initialized = false;

    /// go infinite のとき、stop が来るまで保留する bestmove
    std::string                 _pendingBestmove;

    /// quit が来たか
    bool                        _quit = false;

};

// end namespace 'game'
}

#endif
