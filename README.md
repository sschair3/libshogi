# libshogi

## REQUIREMENTS

- x64
- POSIX system that supports Berkeley sockets interface
- gcc 5.1 or higher that supports c++14

We tested on Fedora25, Ubuntu 16.04 and Linux Mint 18. For users
newly installing OS for libshogi, Linux Mint 18 is recommended.
For an optimal environment, choose the minimal option on the
installation of Fedora 25 Server and yum install 'gcc-c++',
'make' and 'ctags' afterwards. It takes only 20 min approximately
to get ready to use the software. libshogi should work on other
POSIX systems such as xBSDs although we have not tested yet.

## macOS (Apple Silicon / Intel)

libshogiはXcode Command Line Tools (Apple clang) を使ってmacOS
でもビルドできます。まだインストールしていない場合は、先に
インストールしてください。

```
   $ xcode-select --install
```

あとはLinuxと同じ手順でビルド・インストールできます。

```
   $ make
   $ sudo make install
```

macOSでは、ライブラリは `lib64/libshogi.so.0` ではなく
`$(TARGET)/lib/libshogi.0.dylib` (およびシンボリックリンク
`libshogi.dylib`) としてインストールされます。sudoを使わずに
インストールしたい場合は別のプレフィックスを指定し、サンプル
プログラムのビルド時にも同じプレフィックスを渡してください。

```
   $ make install TARGET=$HOME/.local
   $ make -C example/dfpn PREFIX=$HOME/.local
```

移植に関する補足:

- x86以外のCPU (Apple Silicon) では、ビット演算はコンパイラの
  組み込み関数で実装され、PEXTはソフトウェアでエミュレートされ
  ます。`BMI2 = y` はx86-64でのみ有効です。
- macOSには `pthread_spinlock_t` と名前なしPOSIXセマフォが
  ないため、`SpinLock` は `std::atomic_flag` を、`Semaphore` は
  `std::mutex`/`std::condition_variable` を使って実装しています。
  `LIBSHOGI_PORTABLE_SYNC` を定義すると、他の環境でも同じ実装を
  使えます。

## INSTALL

Entering

```
   $ make
```

will compile the source codes. Then, install the library to your
host.

```
   $ sudo make install
```

## RECIPE

libshogi/Makefile has several options.

```Makefile
   TARGET  = /usr/local
   HEADDIR = $(TARGET)/include/shogi/
   LIBRDIR = $(TARGET)/lib64
   BMI2    = n
   TLS     = n
```

TARGET is a top directory the library is installed to. HEADDIR
is a subsidiary directory the header files are placed. LIBDIR
is also a tributary for the library file.

If your processor supports BMI2 (`cat /proc/cpuinfo | grep bmi2`
says something - on linux) you should use native PEXT instruction
for the better performance. To do this, specify the option as:

```
   BMI2    = y
```

We do not suppose libshogi to be used with frequent copying
operation about the position. If the cost of copying is still
an issue in your program, you can make the context cache^1 TLS.
Using TLS, there is some performance penalty because you have to
check the address each time you access it. To do this, specify
the option as:

```
   TLS     = y
```

1:context cache
doesn't hold information of positions. It contains the checking
pieces, number of checks, pinned pieces and etc. This varies
depending on contexts and require some amount of memory
(approx. 500 bytes per instance).

## About this fork

This is a modified version of libshogi by Hiroki Takada.
Modified by 伊藤匠吾 in October 2026.

Main changes:

- Ported to macOS (Apple Silicon)
- Added USI protocol and SFEN support (lib/shogi/Usi.h, Usi.cpp)
- Added Position::attacked() and Position::lastMoveIsLegal()

Other modified files (build settings and tests):
Makefile, lib/Makefile, example/Makefile, example/dfpn/Makefile,
regression/Makefile, regression/Test\*.cpp

License: GNU LGPL v3 (see COPYING and COPYING.GPL)
