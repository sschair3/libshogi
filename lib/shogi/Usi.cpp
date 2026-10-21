/**
 *****************************************************************************

 @file       Usi.cpp

 @brief      USI protocol and SFEN support

 @author     Shogo Ito

 @date       2026-10-22

 @version    $Id:$


  Copyright 2026 Shogo Ito.

  This file is part of libshogi.

  libshogi is free software: you can redistribute it and/or modify it under
  the terms of the GNU Lesser General Public License as published by the
  Free Software Foundation, either version 3 of the License, or (at your
  option) any later version.

  libshogi is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
  License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with libshogi. If not, see <http://www.gnu.org/licenses/>.


  ----------------------------------------------------------------------------
  RELEASE NOTE :

   DATE          REV    REMARK
  ============= ====== =======================================================
   2nd Oct 2026  0.1   Initial release (USI protocol and SFEN support)

 *****************************************************************************/

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

#include <Array.h>
#include <Usi.h>

// begin namespace 'game'
namespace game
{

    /* ------------------------------ 駒と文字の対応 ------------------------------ */

    /// 駒の種類 (FU=1 ... OU=8) → USI/SFEN の文字 (先手の大文字)
    static const char KindToChar[] = {
        //   FU   KY   KE   GI   KA   HI   KI   OU
        ' ', 'P', 'L', 'N', 'S', 'B', 'R', 'G', 'K'};

    /**
     * USI/SFEN の文字 (大文字) → 駒の種類。該当しなければ Piece::EMP
     */
    static Piece::Piece charToKind(char c)
    {
        for (Piece::Piece k = Piece::FU; k <= Piece::OU; ++k)
        {
            if (KindToChar[k] == c)
            {
                return k;
            }
        }
        return Piece::EMP;
    }

    /**
     * 筋・段 (どちらも 0 始まり) → マス
     */
    static inline Square::Square toSquare(int file, int rank)
    {
        return Square::fileAll[file] | Square::rankAll[rank];
    }

    /**
     * USI のマス表記 ("7g" など) を読む
     * @return 読めたら true
     */
    static bool parseSquare(char f, char r, Square::Square &sq)
    {
        if (f < '1' || f > '9' || r < 'a' || r > 'i')
        {
            return false;
        }
        sq = toSquare(f - '1', r - 'a');
        return true;
    }

    /**
     * マス → USI のマス表記
     */
    static std::string squareToUSI(Square::Square sq)
    {
        std::string s;
        s += static_cast<char>('1' + Square::toFile(sq));
        s += static_cast<char>('a' + Square::toRank(sq));
        return s;
    }

    /* -------------------------------- 指し手の変換 -------------------------------- */

    // begin namespace 'game::Usi'
    namespace Usi
    {

        const std::string StartSFEN =
            "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1";

        std::string toUSI(Move::Move m)
        {
            if (Move::isNull(m))
            {
                return "none";
            }

            std::string s;
            if (m & Move::Drop)
            {
                // 駒打ち: 移動元の場所に駒の種類が入っている
                s += KindToChar[Move::from(m)];
                s += '*';
            }
            else
            {
                s += squareToUSI(Move::from(m));
            }
            s += squareToUSI(Move::to(m));
            if (m & Move::Promote)
            {
                s += '+';
            }
            return s;
        }

        Move::Move parseUSI(const std::string &s)
        {
            Square::Square from, to;

            // 駒打ち: "P*5e"
            if (s.size() == 4 && s[1] == '*')
            {
                Piece::Piece k = charToKind(s[0]);
                if (k == Piece::EMP || k == Piece::OU || !parseSquare(s[2], s[3], to))
                {
                    return Move::None;
                }
                return Move::drop(k, to);
            }

            // 移動: "7g7f" / 成り: "8h2b+"
            if (s.size() == 4 || (s.size() == 5 && s[4] == '+'))
            {
                if (!parseSquare(s[0], s[1], from) || !parseSquare(s[2], s[3], to))
                {
                    return Move::None;
                }
                return s.size() == 5 ? Move::promote(from, to) : Move::move(from, to);
            }

            return Move::None;
        }

        /**
         * 成らずに移動した駒が、その先で動けなくならないか (行き所のない駒の禁止)
         */
        static bool canStay(const Position &p, Move::Move m)
        {
            Piece::Piece kind = Piece::neutral(p.square(Move::from(m)));
            int rank = Square::toRank(Move::to(m));
            // 後手なら段を反転して先手と同じ判定にする
            if (p.turn() == Color::White)
            {
                rank = 8 - rank;
            }
            if ((kind == Piece::FU || kind == Piece::KY) && rank == 0)
            {
                return false;
            }
            if (kind == Piece::KE && rank <= 1)
            {
                return false;
            }
            return true;
        }

        bool fromUSI(Position &p, const std::string &s, Move::Move &m)
        {
            Move::Move raw = parseUSI(s);
            if (Move::isNull(raw))
            {
                return false;
            }

            foundation::Array<Move::Move, Move::Max> moves;
            p.genMove(moves);

            // 合法手の中に同じ手があるか (評価値などの上位ビットは無視して比べる)
            for (auto mv : moves)
            {
                if ((mv & Move::MoveMask) == raw)
                {
                    m = mv;
                    return true;
                }
            }

            // libshogi の genMove() は「成れるのに成らない手」の一部を生成しない。
            // 成る手が合法手にあれば、成らない手も (行き所のない駒でなければ) 合法。
            if (!(raw & Move::Drop) && !(raw & Move::Promote))
            {
                auto promoted = static_cast<Move::Move>(raw | Move::Promote);
                for (auto mv : moves)
                {
                    if ((mv & Move::MoveMask) == promoted && canStay(p, raw))
                    {
                        m = raw;
                        return true;
                    }
                }
            }
            return false;
        }

        double Limits::thinkTime(Color::Color turn) const
        {
            if (infinite)
            {
                return 0.0;
            }
            long remain = (turn == Color::Black) ? btime : wtime;
            long inc = (turn == Color::Black) ? binc : winc;
            // 戻り値は「秒」。0 以下は「時間制限なし」の意味になってしまうので、
            // 余裕を引いた結果が小さくなりすぎないように下限を付ける。
            if (byoyomi > 0)
            {
                // 秒読みから 1.1 秒の余裕を取る (ただし秒読みの半分は必ず使う)
                double ms = std::max(static_cast<double>(byoyomi) - 1100.0,
                                     static_cast<double>(byoyomi) * 0.5);
                return ms / 1000.0;
            }
            if (remain > 0 || inc > 0)
            {
                // 残り時間の 1/75 + 加算から 0.5 秒の余裕を取る (最低 10 ミリ秒)
                double ms = static_cast<double>(remain) / 75.0 + static_cast<double>(inc) - 500.0;
                return std::max(ms, 10.0) / 1000.0;
            }
            return 0.0;
        }

        // end namespace 'game::Usi'
    }

    /* ------------------------------ SFEN (Position) ------------------------------ */

    /**
     * SFEN 形式の局面を設定する
     * 例: "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"
     * @param sfen 盤面 手番 持ち駒 [手数]
     * @return 正しく読めたら true (読めなかったときは局面を変更しない)
     */
    bool Position::setSFEN(const std::string &sfen)
    {
        std::istringstream is(sfen);
        std::string board, side, hands;
        int ply = 1;
        if (!(is >> board >> side >> hands))
        {
            return false;
        }
        is >> ply;

        // まず一時的な配列に読み込んで、全部正しいことを確かめてから反映する
        Piece::Piece b[Square::SQVD];
        for (auto s : Square::all)
        {
            b[s] = Piece::EMP;
        }

        // 盤面: 1 段目から、各段は 9 筋 → 1 筋の順
        int rank = 0, file = 8;
        bool promote = false;
        for (char c : board)
        {
            if (c == '/')
            {
                if (file != -1 || promote)
                {
                    return false;
                }
                ++rank;
                file = 8;
                continue;
            }
            if (c >= '1' && c <= '9')
            {
                file -= c - '0';
                if (file < -1 || promote)
                {
                    return false;
                }
                continue;
            }
            if (c == '+')
            {
                promote = true;
                continue;
            }
            Piece::Piece k = charToKind(static_cast<char>(std::toupper(c)));
            if (k == Piece::EMP || file < 0 || rank > 8)
            {
                return false;
            }
            if (promote)
            {
                if (k == Piece::KI || k == Piece::OU)
                {
                    return false;
                }
                k |= Piece::Promoted;
            }
            Color::Color color = std::isupper(c) ? Color::Black : Color::White;
            b[toSquare(file, rank)] = Piece::polar(k, color);
            --file;
            promote = false;
        }
        if (rank != 8 || file != -1)
        {
            return false;
        }

        // 手番
        if (side != "b" && side != "w")
        {
            return false;
        }

        // 持ち駒: "-" または "S2Pb3p" のように (枚数)駒 の並び
        int h[Color::Colors][Piece::Kind] = {};
        if (hands != "-")
        {
            int count = 0;
            for (char c : hands)
            {
                if (c >= '0' && c <= '9')
                {
                    count = count * 10 + (c - '0');
                    continue;
                }
                Piece::Piece k = charToKind(static_cast<char>(std::toupper(c)));
                if (k == Piece::EMP || k == Piece::OU)
                {
                    return false;
                }
                Color::Color color = std::isupper(c) ? Color::Black : Color::White;
                h[color][k] += (count == 0 ? 1 : count);
                count = 0;
            }
            // 枚数の上限 (FU 18, KY/KE/GI/KI 4, KA/HI 2)
            const int maxHand[Piece::Kind] = {0, 18, 4, 4, 4, 2, 2, 4};
            for (auto c : {Color::Black, Color::White})
            {
                for (auto k : Piece::hand)
                {
                    if (h[c][k] > maxHand[k])
                    {
                        return false;
                    }
                }
            }
        }

        // ---- ここから局面に反映 (Position::init() と同じ手順) ----
        _clear();

        for (auto s : Square::all)
        {
            auto p = b[s];
            auto c = Piece::color(p);
            _board[s] = static_cast<unsigned char>(p);
            _bbord[p] ^= Bitboard::Square[s];
            _piece[c] ^= (p != Piece::EMP ? Bitboard::Square[s] : Bitboard::Zero);
            _exchg += _pieceVal[p];
        }
        _ocupd = ~_bbord[Piece::EMP];
        _empty = _bbord[Piece::EMP];

        for (auto c : {Color::Black, Color::White})
        {
            for (auto k : Piece::hand)
            {
                for (int i = 0; i < h[c][k]; ++i)
                {
                    int &d = _hands[c][k];
                    _exchg -= _handsVal[d++][c][k];
                    _exchg += _handsVal[d][c][k];
                }
            }
        }

        _kingSB = Square::SQVD;
        _kingSW = Square::SQVD;
        if (_bbord[Piece::BOU])
        {
            _kingSB = _bbord[Piece::BOU].lsb();
        }
        if (_bbord[Piece::WOU])
        {
            _kingSW = _bbord[Piece::WOU].lsb();
        }

        if (side == "b")
        {
            _last = Color::White;
            _next = Color::Black;
        }
        else
        {
            _last = Color::Black;
            _next = Color::White;
        }

        _numMoves = (ply > 0) ? ply - 1 : 0;

        _hashFull();
        makeCheck();

        return true;
    }

    /**
     * 現在の局面を SFEN 形式の文字列にする
     */
    std::string Position::sfen(void) const
    {
        std::string s;

        // 盤面
        for (int rank = 0; rank < 9; ++rank)
        {
            int empty = 0;
            for (int file = 8; file >= 0; --file)
            {
                Piece::Piece p = _board[toSquare(file, rank)];
                if (p == Piece::EMP)
                {
                    ++empty;
                    continue;
                }
                if (empty > 0)
                {
                    s += static_cast<char>('0' + empty);
                    empty = 0;
                }
                Piece::Piece n = p & Piece::Neutral; // 手番を除いた駒
                Piece::Piece k = (n == Piece::OU) ? n : (n & Piece::Mask);
                if (n != Piece::OU && (n & Piece::Promoted))
                {
                    s += '+';
                }
                char c = KindToChar[k];
                s += (Piece::color(p) == Color::Black) ? c
                                                       : static_cast<char>(std::tolower(c));
            }
            if (empty > 0)
            {
                s += static_cast<char>('0' + empty);
            }
            if (rank < 8)
            {
                s += '/';
            }
        }

        // 手番
        s += (_next == Color::Black) ? " b " : " w ";

        // 持ち駒 (先手 → 後手、それぞれ 飛 角 金 銀 桂 香 歩 の順)
        const Piece::Piece order[] = {
            Piece::HI, Piece::KA, Piece::KI, Piece::GI, Piece::KE, Piece::KY, Piece::FU};
        std::string hands;
        for (auto c : {Color::Black, Color::White})
        {
            for (auto k : order)
            {
                int n = _hands[c][k];
                if (n == 0)
                {
                    continue;
                }
                if (n > 1)
                {
                    hands += std::to_string(n);
                }
                char ch = KindToChar[k];
                hands += (c == Color::Black) ? ch : static_cast<char>(std::tolower(ch));
            }
        }
        s += hands.empty() ? "-" : hands;

        // 手数
        s += " " + std::to_string(_numMoves + 1);
        return s;
    }

    /* ---------------------------- USI プロトコル処理 ---------------------------- */

    const std::string USIConnection::USI_COMMAND = "usi";
    const std::string USIConnection::ISREADY_COMMAND = "isready";
    const std::string USIConnection::SETOPTION_COMMAND = "setoption";
    const std::string USIConnection::USINEWGAME_COMMAND = "usinewgame";
    const std::string USIConnection::POSITION_COMMAND = "position";
    const std::string USIConnection::GO_COMMAND = "go";
    const std::string USIConnection::STOP_COMMAND = "stop";
    const std::string USIConnection::PONDERHIT_COMMAND = "ponderhit";
    const std::string USIConnection::GAMEOVER_COMMAND = "gameover";
    const std::string USIConnection::QUIT_COMMAND = "quit";

    USIConnection::USIConnection(const std::string &name, const std::string &author)
        : _name(name), _author(author)
    {
    }

    void USIConnection::run(std::istream &in)
    {
        std::string line;
        // 入力が終わる (GUI が閉じる) か quit が来たら終了
        while (!_quit && std::getline(in, line))
        {

            std::vector<std::string> command = split(line);
            if (command.empty())
            {
                continue;
            }

            const std::string &cmd = command[0];
            if (cmd == USI_COMMAND)
                _usi();
            else if (cmd == ISREADY_COMMAND)
                _isready();
            else if (cmd == SETOPTION_COMMAND)
                _setoption(command);
            else if (cmd == USINEWGAME_COMMAND)
                _usinewgame();
            else if (cmd == POSITION_COMMAND)
                _position(command);
            else if (cmd == GO_COMMAND)
                _go(command);
            else if (cmd == STOP_COMMAND)
                _stop();
            else if (cmd == PONDERHIT_COMMAND)
            { /* TODO: ponder 未対応 */
            }
            else if (cmd == GAMEOVER_COMMAND)
                _gameover(command);
            else if (cmd == QUIT_COMMAND)
                _quit = true;
            // 知らないコマンドは無視する (USI の決まり)
        }
    }

    std::vector<std::string> USIConnection::split(const std::string &s)
    {
        std::vector<std::string> words;
        std::istringstream is(s);
        std::string w;
        while (is >> w)
        {
            words.push_back(w);
        }
        return words;
    }

    void USIConnection::_initialize(void)
    {
        if (_initialized)
        {
            return;
        }
        Shogi::initialize();
        Position::setValue(Evaluation::Value);
        Position::handsValue(Evaluation::Hands);
        _pos.init();
        _initialized = true;
    }

    void USIConnection::_usi(void)
    {
        std::cout << "id name " << _name << std::endl;
        std::cout << "id author " << _author << std::endl;
        options(std::cout);
        std::cout << "usiok" << std::endl;
    }

    void USIConnection::_isready(void)
    {
        _initialize();
        ready();
        std::cout << "readyok" << std::endl;
    }

    // setoption name <名前> value <値>  (名前・値に空白が入る場合にも対応)
    void USIConnection::_setoption(const std::vector<std::string> &args)
    {
        std::string name, value;
        std::string *target = nullptr;
        for (size_t i = 1; i < args.size(); ++i)
        {
            if (args[i] == "name")
            {
                target = &name;
                continue;
            }
            if (args[i] == "value")
            {
                target = &value;
                continue;
            }
            if (target != nullptr)
            {
                *target += (target->empty() ? "" : " ") + args[i];
            }
        }
        setOption(name, value);
    }

    void USIConnection::_usinewgame(void)
    {
        _initialize();
        _pendingBestmove.clear();
        newGame();
    }

    // position startpos [moves m1 m2 ...]
    // position sfen <盤面> <手番> <持ち駒> <手数> [moves m1 m2 ...]
    void USIConnection::_position(const std::vector<std::string> &args)
    {
        _initialize();

        // 最後まで正しく反映できたときだけ true に戻す。
        // 途中で失敗したら、_pos は前の局面や途中までの局面のままなので使わない
        _positionValid = false;

        if (args.size() < 2)
        {
            std::cout << "info string invalid position command" << std::endl;
            return;
        }

        size_t i = 1;
        if (args[i] == "startpos")
        {
            _pos.init();
            ++i;
        }
        else if (args[i] == "sfen")
        {
            std::string sfen;
            for (++i; i < args.size() && args[i] != "moves"; ++i)
            {
                sfen += (sfen.empty() ? "" : " ") + args[i];
            }
            if (!_pos.setSFEN(sfen))
            {
                std::cout << "info string invalid sfen: " << sfen << std::endl;
                return;
            }
        }
        else
        {
            std::cout << "info string invalid position command: " << args[i] << std::endl;
            return;
        }

        if (i < args.size() && args[i] == "moves")
        {
            for (++i; i < args.size(); ++i)
            {
                Move::Move m;
                if (!Usi::fromUSI(_pos, args[i], m))
                {
                    std::cout << "info string illegal move: " << args[i] << std::endl;
                    return;
                }
                _pos.move(m);
            }
        }

        _positionValid = true;
    }

    // go [btime x] [wtime x] [byoyomi x] [binc x] [winc x] [infinite]
    void USIConnection::_go(const std::vector<std::string> &args)
    {
        _initialize();

        Usi::Limits limits;
        for (size_t i = 1; i < args.size(); ++i)
        {
            auto next = [&]()
            {
                return (i + 1 < args.size()) ? std::atol(args[++i].c_str()) : 0L;
            };
            if (args[i] == "btime")
                limits.btime = next();
            else if (args[i] == "wtime")
                limits.wtime = next();
            else if (args[i] == "byoyomi")
                limits.byoyomi = next();
            else if (args[i] == "binc")
                limits.binc = next();
            else if (args[i] == "winc")
                limits.winc = next();
            else if (args[i] == "infinite")
                limits.infinite = true;
        }

        // 局面が正しく設定できていないときは、間違った局面で指さずに投了する
        std::string bestmove;
        if (!_positionValid)
        {
            std::cout << "info string position is invalid, resign" << std::endl;
            bestmove = "resign";
        }
        else
        {
            Move::Move best = think(_pos, limits);
            bestmove = Move::isNull(best) ? "resign" : Usi::toUSI(best);
        }

        if (limits.infinite)
        {
            // go infinite のときは stop が来てから bestmove を返す決まり
            _pendingBestmove = bestmove;
        }
        else
        {
            std::cout << "bestmove " << bestmove << std::endl;
        }
    }

    void USIConnection::_stop(void)
    {
        // TODO: 探索を別スレッドにして、探索の途中で止められるようにする
        if (!_pendingBestmove.empty())
        {
            std::cout << "bestmove " << _pendingBestmove << std::endl;
            _pendingBestmove.clear();
        }
    }

    void USIConnection::_gameover(const std::vector<std::string> &args)
    {
        _pendingBestmove.clear();
        gameOver(args.size() > 1 ? args[1] : "");
    }

    void USIConnection::info(const Position &p, int depth, Evaluation::Eval value,
                             uint64_t nodes, double seconds, Move::Move best) const
    {
        // USI の評価値は「手番側から見た値」なので、後手番なら符号を反転
        Evaluation::Eval v = (p.turn() == Color::Black) ? value : -value;

        std::cout << "info depth " << depth;
        if (v >= Evaluation::MateThreshold)
        {
            std::cout << " score mate " << (Evaluation::Infinity - v);
        }
        else if (v <= -Evaluation::MateThreshold)
        {
            std::cout << " score mate -" << (Evaluation::Infinity + v);
        }
        else
        {
            std::cout << " score cp " << v;
        }
        std::cout << " nodes " << nodes
                  << " time " << static_cast<long>(seconds * 1000.0);
        if (!Move::isNull(best))
        {
            std::cout << " pv " << Usi::toUSI(best);
        }
        std::cout << std::endl;
    }

    // end namespace 'game'
}
