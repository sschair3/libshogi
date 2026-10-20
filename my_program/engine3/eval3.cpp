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
    static const char *const PSTFile = "/Users/shogo/shogi/learn/pst3.txt";

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

    Eval evaluate(Position &p)
    {
        Eval value = locationEvaluation(p);

        // 持ち駒 (FU〜KI)
        for (Piece::Piece kind = Piece::FU; kind <= Piece::KI; ++kind)
        {
            value += Hand[kind] * (p.hand(Color::Black, kind) - p.hand(Color::White, kind));
        }

        // 手番
        value += p.turn() == Color::Black ? Turn : -Turn;

        return value;
    }

}
