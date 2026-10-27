/**
 * bench.cpp
 *
 * エンジンの探索の速さ (NPS: 1 秒あたりの探索ノード数) を測る。
 * 決まった局面を、決まった深さで USI 経由で探索させ、ノード数と時間を集計する。
 *
 * 使い方:
 *   ./bench <エンジンのパス> [深さ] [設定名=値 ...]
 *   例) ./bench engine2/engine2 6
 *       ./bench <やねうら王> 10 EvalDir=/path/to/eval BookFile=no_book Threads=1
 *
 * 深さは Depth (自作エンジン用) と DepthLimit (やねうら王用) の両方で送る。
 */

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "EngineProcess.h"

/// 測定する局面 (floodgate の対局 2 局から、24・48・72・96 手目を取り出したもの + 平手)
static const std::vector<std::string> Positions = {
    "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1",
    "lnkg3nl/2s1gr3/1pp1psbpp/p2p1pp2/2P1P2P1/7R1/PP1PSPP1P/1BK1G4/LNSG3NL b - 25",
    "l6nl/1kg2r3/1pnsp1bp1/2gp1sp1p/pPp1Pp1P1/3SR4/PKNP1PP1P/1B2G1G2/L1S4NL b P 49",
    "l7l/1kr6/1g1sp1n2/1p1p1s+Bpp/pnK1Pp3/9/P1NPRPP1P/1B2G1G2/L1S4NL b S2Pg3p 73",
    "l7l/1k7/1g1s2n2/1p1p1s3/p1r1ppppp/2+n6/P2PKPP1P/1B2G1GR1/L1S2s1NL b Nbg5p 97",
    "lnkg3nl/2s1g2r1/1p1ppsbpp/p1p2pp2/7P1/P1PPP4/1PB2PP1P/1K2GS1R1/LNSG3NL b - 25",
    "l2g3nl/1kg4r1/1sn1p1bpp/ppp1s1p2/5p1P1/P1PGP1P2/1P2BP2P/LSG4R1/KNS4NL b Pp 49",
    "lkg4nl/s1gr5/4p2+Pp/pPp1sbp2/2Pn1p3/PpG1P1P2/5P2P/LSGR1B3/KNS4+pL b Pnp 73",
    "lkg4nl/s1g4+B1/4+P3p/pPPps4/1ppn1p3/Pl2P1P2/2s2PB1P/LGG+n5/KNS5+p b R2Prp 97",
};

/// 同じ局面を何回探索させるか (時間は一番短かった回を使い、たまたまの遅れを除く)
static const int Repeat = 3;

/// 1 局面の探索をいつまで待つか (ミリ秒)。深さで止まるので、普通はここまでかからない
static const int TimeoutMs = 600000;

/// "info ... nodes 12345 ..." の行から nodes の値を取り出す (なければ -1)
static long long nodesOf(const std::string &info)
{
    std::istringstream is(info);
    std::string w;
    while (is >> w)
    {
        if (w == "nodes" && is >> w)
        {
            return std::stoll(w);
        }
    }
    return -1;
}

/// "info depth 6 ..." の行から depth の値を取り出す (なければ 0)
static int depthOf(const std::string &info)
{
    std::istringstream is(info);
    std::string w;
    while (is >> w)
    {
        if (w == "depth" && is >> w)
        {
            return std::stoi(w);
        }
    }
    return 0;
}

int main(int argc, char *argv[])
{
    signal(SIGPIPE, SIG_IGN);

    if (argc < 2)
    {
        std::cerr << "使い方: " << argv[0] << " <エンジンのパス> [深さ] [設定名=値 ...]" << std::endl;
        return 1;
    }
    std::string path = argv[1];
    int depth = (argc >= 3) ? std::atoi(argv[2]) : 6;

    EngineProcess engine;
    if (!engine.start(path))
    {
        return 1;
    }

    // usi → usiok (途中の id name を覚えておく)
    std::string name, line;
    engine.send("usi");
    while (true)
    {
        if (!engine.readLine(line, 5000))
        {
            std::cerr << "usiok が来ない" << std::endl;
            return 1;
        }
        if (line.rfind("id name ", 0) == 0)
        {
            name = line.substr(8);
        }
        if (line == "usiok")
        {
            break;
        }
    }

    // 設定: 深さ + コマンドラインで指定されたもの ("名前=値")
    engine.send("setoption name Depth value " + std::to_string(depth));
    engine.send("setoption name DepthLimit value " + std::to_string(depth));
    for (int i = 3; i < argc; ++i)
    {
        std::string opt = argv[i];
        size_t eq = opt.find('=');
        if (eq == std::string::npos)
        {
            std::cerr << "設定は 名前=値 の形で書いてください: " << opt << std::endl;
            return 1;
        }
        engine.send("setoption name " + opt.substr(0, eq) + " value " + opt.substr(eq + 1));
    }

    engine.send("isready");
    if (!engine.waitFor("readyok", 30000))
    {
        std::cerr << "readyok が来ない" << std::endl;
        return 1;
    }
    engine.send("usinewgame");

    std::cout << name << "  (深さ " << depth << ", 各局面 " << Repeat << " 回のうち最速)\n\n";
    std::cout << " 局面  深さ       ノード数    時間(ms)          NPS\n";

    long long totalNodes = 0;
    double totalMs = 0.0;

    for (size_t i = 0; i < Positions.size(); ++i)
    {
        long long nodes = -1;
        int reached = 0;
        double bestMs = 1e18;

        for (int r = 0; r < Repeat; ++r)
        {
            engine.send("position sfen " + Positions[i]);
            auto start = Clock::now();
            engine.send("go byoyomi " + std::to_string(TimeoutMs));

            // bestmove が来るまで読み、最後の "info depth ... nodes ..." を覚えておく
            std::string lastInfo;
            while (true)
            {
                if (!engine.readLine(line, TimeoutMs))
                {
                    std::cerr << "bestmove が来ない" << std::endl;
                    return 1;
                }
                if (line.rfind("info depth", 0) == 0 && nodesOf(line) >= 0)
                {
                    lastInfo = line;
                }
                if (line.rfind("bestmove", 0) == 0)
                {
                    break;
                }
            }
            double ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();

            nodes = nodesOf(lastInfo);
            reached = depthOf(lastInfo);
            if (ms < bestMs)
            {
                bestMs = ms;
            }
        }

        totalNodes += nodes;
        totalMs += bestMs;
        std::cout << std::setw(5) << (i + 1)
                  << std::setw(6) << reached
                  << std::setw(15) << nodes
                  << std::setw(12) << std::fixed << std::setprecision(1) << bestMs
                  << std::setw(13) << static_cast<long long>(static_cast<double>(nodes) / (bestMs / 1000.0))
                  << "\n";
    }

    std::cout << "\n合計  ノード数 " << totalNodes
              << "  時間 " << std::fixed << std::setprecision(1) << totalMs << " ms"
              << "  NPS " << static_cast<long long>(static_cast<double>(totalNodes) / (totalMs / 1000.0)) << std::endl;

    engine.quit();
    return 0;
}
