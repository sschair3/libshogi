/**
 * eval3.cpp
 *
 * 評価部の実装 (棋譜から学習した表 pst3.txt を読み込んで使う)。
 *
 *   評価値 = Σ 盤上の駒 PST[駒の種類][マス]
 *          + Σ 持ち駒   Hand[駒の種類] × 枚数
 *          ± 手番のボーナス (先手番なら +, 後手番なら -)
 *
 * PST の値は駒の価値を含んでいるので、駒割り (p.eval()) は足さない。
 */

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
    static const char *const PSTFile = "/Volumes/SSD/shogi/learn/pst3.txt";

    /// 盤上の駒: [駒の種類][マス番号] (先手の駒の値。後手は 80 - sq を引く)
    static Eval PST[16][81];

    /// 持ち駒 1 枚の価値: [駒の種類]
    static Eval Hand[16];

    /// 手番のボーナス
    static Eval Turn;

    void initialize()
    {
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
     * 手 m を指したときの評価値の増減 (先手から見た値、手番のボーナスは含まない)
     * p.move(m) の「前」に呼ぶこと (動かす駒・取る駒を盤から読むため)
     *
     *   評価値 (手番のボーナスを除く) を E とすると
     *     E(m を指した後) = E(m を指す前) + moveDelta(p, m)
     */
    Eval moveDelta(const Position &p, Move::Move m)
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

    Eval evaluate(Position &p)
    {
        Eval value = locationEvaluation(p);

        // 持ち駒 (FU〜KI)
        for (Piece::Piece kind = Piece::FU; kind <= Piece::KI; ++kind)
        {
            value += Hand[kind] * (p.hand(Color::Black, kind) - p.hand(Color::White, kind));
        }

        // 手番
        value += turnBonus(p.turn());

        return value;
    }

}
