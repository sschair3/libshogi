/**
 * eval.cpp (engine5)
 *
 * 評価部の実装 (learn/train_kp.py が学習した pst_kp.txt と kp.bin を読み込んで使う)。
 *
 *   評価値 = Σ 盤上の駒 PST[駒の種類][マス]
 *          + Σ 持ち駒   Hand[駒の種類] × 枚数
 *          + KP (玉の位置 × 駒 × マス。先手玉から見た分 − 後手玉から見た分)
 *          ± 手番のボーナス (先手番なら +, 後手番なら -)
 *
 * PST の値は駒の価値を含んでいるので、駒割り (p.eval()) は足さない。
 */

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>

#include "eval.h"

using namespace game;
using namespace Evaluation;

namespace evaluation
{

    /**
     * 学習した表のファイル (learn/train.py の save() が書き出す形式)
     *   1〜16 行目: 駒の種類 0〜15 の 81 マスぶん (libshogi のマス番号 = 筋 × 9 + 段 の順)
     *   17 行目   : 持ち駒 1 枚の価値 (駒の種類 0〜15)
     *   18 行目   : 手番のボーナス
     * GUI から起動すると作業ディレクトリが決まらないので、絶対パスで書く。
     */
    static const char *const PSTFile = "/Volumes/SSD/shogi/learn/pst_kp.txt";

    /**
     * KP の表のファイル (learn/train_kp.py の save_kp() が書き出す形式)
     *   int32 (リトルエンディアン) が KPSize 個。値は評価値の単位
     *   先頭から: KPBoard[玉のマス 81][r 2][t 13][駒のマス 81]
     *   続いて  : KPHand [玉のマス 81][r 2][持ち駒の種類 7 (歩〜金)]
     *     玉のマス・駒のマスは「玉の持ち主から見たマス」(後手玉なら盤を 180 度回す)
     *     r: 0 = 玉の持ち主の駒、1 = 相手の駒
     *     t: 玉以外の駒の種類 (歩〜金 = 0〜6、と〜龍 = 7〜12)
     *     値は「玉の持ち主の得」(先手玉の分は足し、後手玉の分は引く)
     */
    static const char *const KPFile = "/Volumes/SSD/shogi/learn/kp.bin";

    /// 盤上の駒: [駒の種類][マス番号] (先手の駒の値。後手は 80 - sq を引く)
    static Eval PST[16][81];

    /// 持ち駒 1 枚の価値: [駒の種類]
    static Eval Hand[16];

    /// 手番のボーナス
    static Eval Turn;

    /// KP の表 (int32 で読み込む。Eval も int なので、そのまま使える)
    static const int KPTypes = 13;
    static int32_t KPBoard[81][2][KPTypes][81];
    static int32_t KPHand[81][2][7];

    /// 駒の種類 (libshogi の番号 0〜15) → KP の種類 t (玉・空きは -1)
    static const int KPTypeOf[16] = {-1, 0, 1, 2, 3, 4, 5, 6, -1, 7, 8, 9, 10, 11, 12, -1};

    /// KP の表を読み込む
    static void loadKP()
    {
        std::ifstream ifs(KPFile, std::ios::binary);
        if (!ifs)
        {
            std::cerr << "KP のファイルを開けません: " << KPFile << std::endl;
            std::exit(EXIT_FAILURE);
        }
        // 2 つの配列を、ファイルに並んでいる順にそのまま読み込む (Mac はリトルエンディアン)
        ifs.read(reinterpret_cast<char *>(KPBoard), sizeof(KPBoard));
        ifs.read(reinterpret_cast<char *>(KPHand), sizeof(KPHand));
        // ちょうど読み切れたか (足りなくても、余っていても形式が違う)
        if (!ifs || ifs.peek() != std::ifstream::traits_type::eof())
        {
            std::cerr << "KP のファイルの大きさが違います: " << KPFile << std::endl;
            std::exit(EXIT_FAILURE);
        }
    }

    void initialize()
    {
        loadKP();

        std::ifstream ifs(PSTFile);
        if (!ifs)
        {
            std::cerr << "評価関数のファイルを開けません: " << PSTFile << std::endl;
            std::exit(EXIT_FAILURE);
        }

        for (int k = 0; k < 16; ++k)
        {
            for (int sq = 0; sq < 81; ++sq)
            {
                ifs >> PST[k][sq];
            }
        }
        for (int k = 0; k < 16; ++k)
        {
            ifs >> Hand[k];
        }
        ifs >> Turn;

        if (!ifs)
        {
            std::cerr << "評価関数のファイルの形式が違います: " << PSTFile << std::endl;
            std::exit(EXIT_FAILURE);
        }
    }

    Eval locationEvaluation(const Position &p)
    {
        Eval value = 0;
        for (auto sq : Square::all)
        {
            Piece::Piece pc = p.square(sq);
            if (pc == Piece::EMP)
            {
                continue;
            }
            Piece::Piece kind = pc & Piece::Neutral; // 手番を外した駒の種類
            if (Piece::color(pc) == Color::Black)
            {
                value += PST[kind][sq];
            }
            else
            {
                // 後手の駒は盤を 180 度回して先手の表を引き、符号を反転する
                value -= PST[kind][80 - sq];
            }
        }
        return value;
    }

    /**
     * 手 m による PST と持ち駒の部分の増減 (先手から見た値)
     * p.move(m) の「前」に呼ぶこと (動かす駒・取る駒を盤から読むため)
     */
    static Eval pstDelta(const Position &p, Move::Move m)
    {
        bool black = (p.turn() == Color::Black);
        int s = black ? 1 : -1; // 指す側の符号 (先手の得なら +、後手の得なら -)

        // PST は先手の駒の表なので、後手の駒は盤を 180 度回したマスで引く
        //   mine  : 指す側の駒を引くときのマス
        //   theirs: 相手の駒を引くときのマス
        auto mine = [black](Square::Square sq) { return black ? sq : 80 - sq; };
        auto theirs = [black](Square::Square sq) { return black ? 80 - sq : sq; };

        Square::Square to = Move::to(m);
        Eval d; // 指す側から見た増減

        if (m & Move::Drop)
        {
            // 駒を打つ手: 持ち駒が 1 枚減り、盤上に駒が 1 枚増える
            // 打つ手では、移動元の場所に駒の種類が入っている (型は Square なので Piece に直す)
            Piece::Piece kind = static_cast<Piece::Piece>(Move::from(m));
            d = PST[kind][mine(to)] - Hand[kind];
        }
        else
        {
            // 駒を動かす手: 移動元の点数がなくなり、移動先の点数が加わる
            Square::Square from = Move::from(m);
            Piece::Piece kind = p.square(from) & Piece::Neutral; // 先後の区別を外した種類
            Piece::Piece after = (m & Move::Promote) ? (kind | Piece::Promoted) : kind;
            d = PST[after][mine(to)] - PST[kind][mine(from)];

            // 駒を取る手: 相手の駒が盤から消え、その駒 (成駒なら元の駒) が持ち駒に加わる
            Piece::Piece captured = p.square(to);
            if (captured != Piece::EMP)
            {
                Piece::Piece ckind = captured & Piece::Neutral;
                d += PST[ckind][theirs(to)];       // 相手の点数が減る = 指す側の得
                d += Hand[ckind & Piece::Mask];    // と金 → 歩 のように元の駒に戻る
            }
        }

        return s * d;
    }

    /// 手番のボーナス (先手から見た値): 先手番なら +Turn、後手番なら -Turn
    Eval turnBonus(Color::Color turn)
    {
        return (turn == Color::Black) ? Turn : -Turn;
    }

    /* ---------------------------------------------------------------- KP */
    //
    // KP の評価値 (先手から見た値) = 先手玉から見た分 − 後手玉から見た分
    //
    // 「〜玉から見た分」(以下「片側」) は、その玉の持ち主の得を表す。
    //   k  : 玉のマス (玉の持ち主から見たマス。先手玉はそのまま、後手玉は 80 − マス)
    //   r  : 0 = 玉の持ち主の駒、1 = 相手の駒
    //   駒のマスも玉の持ち主から見たマスにする

    /// 片側の、盤上の駒 1 枚の点数
    static inline Eval sideBoard(bool kingBlack, int k, bool pieceBlack, int t, int sq)
    {
        int r = (pieceBlack == kingBlack) ? 0 : 1;
        return KPBoard[k][r][t][kingBlack ? sq : 80 - sq];
    }

    /// 片側の、持ち駒 1 枚の点数 (kind: 歩〜金)
    static inline Eval sideHand(bool kingBlack, int k, bool handBlack, int kind)
    {
        int r = (handBlack == kingBlack) ? 0 : 1;
        return KPHand[k][r][kind - 1];
    }

    /// 玉のマス (玉の持ち主から見たマス)
    static inline int kingSquare(const Position &p, bool black)
    {
        if (black)
            return p.bitboard(Piece::BOU).lsb();
        return 80 - p.bitboard(Piece::WOU).lsb();
    }

    /**
     * 片側の合計 (盤上の玉以外の駒 + 持ち駒)
     *   skip: このマスの駒は数えない (玉が駒を取る手で、取られる駒を除くため。-1 なら除かない)
     */
    static Eval kpSide(const Position &p, bool kingBlack, int k, int skip = -1)
    {
        Eval value = 0;
        for (auto sq : Square::all)
        {
            Piece::Piece pc = p.square(sq);
            if (pc == Piece::EMP || sq == skip)
                continue;
            int t = KPTypeOf[pc & Piece::Neutral];
            if (t < 0)
                continue; // 玉
            value += sideBoard(kingBlack, k, Piece::color(pc) == Color::Black, t, sq);
        }
        for (Piece::Piece kind = Piece::FU; kind <= Piece::KI; ++kind)
        {
            value += p.hand(Color::Black, kind) * sideHand(kingBlack, k, true, static_cast<int>(kind));
            value += p.hand(Color::White, kind) * sideHand(kingBlack, k, false, static_cast<int>(kind));
        }
        return value;
    }

    /**
     * KP の評価 (先手から見た値)。毎回、盤全体と持ち駒を見て計算する
     * (探索中は使わない。ルート局面の計算と、確認用)
     */
    Eval kpEvaluation(const Position &p)
    {
        return kpSide(p, true, kingSquare(p, true)) - kpSide(p, false, kingSquare(p, false));
    }

    /**
     * 手 m による KP の部分の増減 (先手から見た値)。p.move(m) の「前」に呼ぶこと
     *
     *   玉以外を動かす・打つ手: 玉の位置は変わらないので、増えた駒・減った駒の点数だけ足し引きする
     *   玉を動かす手          : その玉から見た片側は全部変わるので、全部計算し直す
     *                           (相手の玉から見た片側は、駒を取ったときだけ変わる)
     */
    static Eval kpDelta(const Position &p, Move::Move m)
    {
        bool black = (p.turn() == Color::Black); // 指す側
        int kb = kingSquare(p, true);
        int kw = kingSquare(p, false);

        // 駒 1 枚・持ち駒 1 枚の、KP の評価値 (先手から見た値) への寄与
        auto board = [&](bool pieceBlack, int t, int sq)
        { return sideBoard(true, kb, pieceBlack, t, sq) - sideBoard(false, kw, pieceBlack, t, sq); };
        auto hand = [&](bool handBlack, int kind)
        { return sideHand(true, kb, handBlack, kind) - sideHand(false, kw, handBlack, kind); };

        Square::Square to = Move::to(m);

        if (m & Move::Drop)
        {
            // 持ち駒が 1 枚減り、盤上に 1 枚増える
            int kind = static_cast<int>(Move::from(m));
            return board(black, KPTypeOf[kind], to) - hand(black, kind);
        }

        Square::Square from = Move::from(m);
        Piece::Piece kind = p.square(from) & Piece::Neutral;
        Piece::Piece captured = p.square(to);
        Piece::Piece ckind = captured & Piece::Neutral; // 取る駒の種類 (取らないなら EMP = 0)

        if (kind != Piece::OU)
        {
            // 玉以外を動かす手: 移動元の駒が消え、移動先に (成っていれば成駒が) 現れる
            Piece::Piece after = (m & Move::Promote) ? (kind | Piece::Promoted) : kind;
            Eval d = board(black, KPTypeOf[after], to) - board(black, KPTypeOf[kind], from);
            if (captured != Piece::EMP)
            {
                // 相手の駒が盤から消え、元の駒として自分の持ち駒に加わる
                d += -board(!black, KPTypeOf[ckind], to) + hand(black, ckind & Piece::Mask);
            }
            return d;
        }

        // 玉を動かす手
        int oldK = black ? kb : kw;                    // 動かす玉の、今のマス
        int newK = black ? to : 80 - to;               // 動かした後のマス
        int skip = (captured != Piece::EMP) ? to : -1; // 取られる駒は、動かした後には盤にいない

        // 動かした玉から見た片側: 前と後をそれぞれ全部計算する
        Eval before = kpSide(p, black, oldK);
        Eval after = kpSide(p, black, newK, skip);
        if (captured != Piece::EMP)
            after += sideHand(black, newK, black, ckind & Piece::Mask); // 取った駒が持ち駒に加わる

        // 相手の玉から見た片側: 駒を取ったときだけ変わる
        int oppK = black ? kw : kb;
        Eval opp = 0;
        if (captured != Piece::EMP)
        {
            opp = -sideBoard(!black, oppK, !black, KPTypeOf[ckind], to) // 相手の駒が消える
                  + sideHand(!black, oppK, black, ckind & Piece::Mask); // 指す側の持ち駒が増える
        }

        // 片側の値は「その玉の持ち主の得」なので、先手から見た値に直す
        int s = black ? 1 : -1;
        return s * (after - before) - s * opp;
    }

    /**
     * 手 m を指したときの評価値の増減 (先手から見た値、手番のボーナスは含まない)
     * p.move(m) の「前」に呼ぶこと (動かす駒・取る駒を盤から読むため)
     *
     *   評価値 (手番のボーナスを除く) を E とすると
     *     E(m を指した後) = E(m を指す前) + moveDelta(p, m)
     */
    Eval moveDelta(const Position &p, Move::Move m)
    {
        return pstDelta(p, m) + kpDelta(p, m);
    }

    Eval evaluate(Position &p)
    {
        Eval value = locationEvaluation(p);

        // 持ち駒 (FU〜KI)
        for (Piece::Piece kind = Piece::FU; kind <= Piece::KI; ++kind)
        {
            value += Hand[kind] * (p.hand(Color::Black, kind) - p.hand(Color::White, kind));
        }

        // KP
        value += kpEvaluation(p);

        // 手番
        value += turnBonus(p.turn());

        return value;
    }

}
