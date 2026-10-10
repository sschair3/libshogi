
TARGET  = /usr/local
HEADDIR = $(TARGET)/include/shogi/
ifeq ($(shell uname -s),Darwin)
LIBRDIR = $(TARGET)/lib
LIBNAME = libshogi.0.dylib
LIBLINK = libshogi.dylib
else
LIBRDIR = $(TARGET)/lib64
LIBNAME = libshogi.so.0
LIBLINK = libshogi.so
endif
BMI2    = n
TLS     = n

HEADERS = lib/shogi/Bitboard.h lib/shogi/Color.h lib/shogi/Convert.h \
          lib/shogi/Direction.h lib/shogi/Effect.h lib/shogi/Evaluation.h \
          lib/shogi/Move.h lib/shogi/Piece.h lib/shogi/Position.h \
          lib/shogi/Region.h lib/shogi/Shogi.h lib/shogi/Square.h \
          lib/shogi/Zobrist.h \
          lib/foundation/Common.h \
          lib/foundation/Array.h lib/foundation/Atomic.h \
          lib/foundation/BTree.h lib/foundation/HashTree.h \
          lib/foundation/List.h lib/foundation/MaxHeap.h \
          lib/foundation/MinHeap.h lib/foundation/Mutex.h \
          lib/foundation/Semaphore.h lib/foundation/SpinLock.h \
          lib/foundation/Thread.h lib/foundation/Vector.h \
		  lib/foundation/BitOperations.h \
          lib/utility/Utility.h lib/csa/CSASummary.h \
          lib/csa/CSAConnection.h lib/csa/CSAFile.h

PRGRMS  = lib/shogi/Direction.cpp lib/shogi/Region.cpp lib/shogi/Square.cpp \
          lib/shogi/Zobrist.cpp lib/shogi/Effect.cpp \
          lib/shogi/Shogi.cpp lib/shogi/Position.cpp \
          lib/shogi/Bitboard.cpp lib/utility/Utility.cpp \
          lib/csa/CSAConnection.cpp lib/csa/CSAFile.cpp lib/csa/CSASummary.cpp

all: lib/$(LIBNAME) tags 

lib/$(LIBNAME): $(HEADERS) $(PRGRMS)
	CFLAGS=$(CFLAGS) $(MAKE) -C lib LIBNAME=$(LIBNAME) INSTALLNAME=$(LIBRDIR)/$(LIBNAME)

tags: $(HEADERS) $(PRGRMS)
	-ctags -R . 2>/dev/null || true

install: lib/$(LIBNAME)
	mkdir -p $(HEADDIR)
	mkdir -p $(LIBRDIR)
	install -m 644 $(HEADERS) $(HEADDIR)
	install -m 755 lib/$(LIBNAME) $(LIBRDIR)
	ln -fs $(LIBNAME) $(LIBRDIR)/$(LIBLINK)

uninstall:
	rm -rf $(HEADDIR)
	rm -f  $(LIBRDIR)/$(LIBNAME)
	rm -f  $(LIBRDIR)/$(LIBLINK)

clean:
	$(MAKE) -C lib clean LIBNAME=$(LIBNAME)
	$(MAKE) -C example clean
	$(MAKE) -C regression clean
	rm -f tags
