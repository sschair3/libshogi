# $Id:$

ifeq ($(shell uname -s),Darwin)
LIBNAME    ?= libshogi.0.dylib
INSTALLNAME ?= /usr/local/lib/$(LIBNAME)
SHAREDFLAGS = -dynamiclib -install_name $(INSTALLNAME) \
              -compatibility_version 0 -current_version 0
else
LIBNAME    ?= libshogi.so.0
SHAREDFLAGS = -shared -Wl,-soname,$(LIBNAME)
endif

DEBUG   = n
MCHCK   = n
BMI2    = $(strip $(shell grep ^BMI2 ../Makefile | cut -d= -f 2))
TLS     = $(strip $(shell grep ^TLS  ../Makefile | cut -d= -f 2))


# ----------------------------------------------------------------------------
# platform detection (Linux / macOS)
UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)
ifeq ($(UNAME_S),Darwin)
RTLIB     =
ifeq ($(UNAME_M),x86_64)
ARCHFLAGS = -mtune=native
else
ARCHFLAGS =
endif
else
RTLIB     = -lrt
ARCHFLAGS = -mtune=native
endif
# ----------------------------------------------------------------------------

CC      = g++
DSFMT   = dSFMT-src-2.2.3

ifeq ($(BMI2),y)
CFLAGS += -DUSE_NATIVE_PEXTINSTRUCTION
endif

ifeq ($(TLS),y)
CFLAGS += -DUSE_THREADLOCALSTORAGE
endif

ifeq ($(DEBUG),y)
ifeq ($(MCHCK),y)
CFLAGS += -std=c++14 -Wall -Wextra -Wformat=2 -Wstrict-aliasing=2 \
          -Wcast-qual -Wcast-align -Wwrite-strings -Wconversion \
          -Wfloat-equal -Wpointer-arith -Wswitch-enum \
          -O0 -g $(ARCHFLAGS) \
          -ftrapv -fthreadsafe-statics \
          -fsanitize=address \
          -I./ -Ifoundation -I$(DSFMT) -DDSFMT_MEXP=19937 \
          -I./csa -I./utility -I./shogi \
          -D_FOUNDATION_ARRAY_DEBUG \
          -D_FOUNDATION_ATOMIC_DEBUG  -D_FOUNDATION_BTREE_DEBUG \
          -D_FOUNDATION_HASHTREE_DEBUG -D_FOUNDATION_LIST_DEBUG \
          -D_FOUNDATION_SEMAPHORE_DEBUG -D_FOUNDATION_SPINLOCK_DEBUG \
          -D_FOUNDATION_THREAD_DEBUG -D_FOUNDATION_VECTOR_DEBUG \
          -D_UTILITY_DEBUG \
          -D_GAME_CSACONNECTION_DEBUG
LFLAGS  =  -pthread $(RTLIB)
else
CFLAGS += -std=c++14 -Wall -Wextra -Wformat=2 -Wstrict-aliasing=2 \
          -Wcast-qual -Wcast-align -Wwrite-strings -Wconversion \
          -Wfloat-equal -Wpointer-arith -Wswitch-enum \
          -O0 -g $(ARCHFLAGS) \
          -ftrapv -fthreadsafe-statics \
          -I./ -Ifoundation -I$(DSFMT) -DDSFMT_MEXP=19937 \
          -I./csa -I./utility -I./shogi \
          -D_FOUNDATION_ARRAY_DEBUG \
          -D_FOUNDATION_ATOMIC_DEBUG  -D_FOUNDATION_BTREE_DEBUG \
          -D_FOUNDATION_HASHTREE_DEBUG -D_FOUNDATION_LIST_DEBUG \
          -D_FOUNDATION_SEMAPHORE_DEBUG -D_FOUNDATION_SPINLOCK_DEBUG \
          -D_FOUNDATION_THREAD_DEBUG -D_FOUNDATION_VECTOR_DEBUG \
          -D_UTILITY_DEBUG \
          -D_GAME_CSACONNECTION_DEBUG
LFLAGS  = -pthread $(RTLIB)
endif
else
ifeq ($(MCHCK),y)
CFLAGS += -std=c++14 -Wall -Wextra -Wformat=2 -Wstrict-aliasing=2 \
          -Wcast-qual -Wcast-align -Wwrite-strings -Wconversion \
          -Wfloat-equal -Wpointer-arith -Wswitch-enum \
          -O3 -g $(ARCHFLAGS) -funroll-loops \
          -fthreadsafe-statics -ftrapv -fsanitize=address \
          -I./ -Ifoundation -I$(DSFMT) -DDSFMT_MEXP=19937 \
          -I./csa -I./utility -I./shogi
LFLAGS  = -pthread $(RTLIB)
else
CFLAGS += -std=c++14 -Wall -Wextra -Wformat=2 -Wstrict-aliasing=2 \
          -Wcast-qual -Wcast-align -Wwrite-strings -Wconversion \
          -Wfloat-equal -Wpointer-arith -Wswitch-enum \
          -O3 -g $(ARCHFLAGS) -funroll-loops \
          -fthreadsafe-statics \
          -I./ -Ifoundation -I$(DSFMT) -DDSFMT_MEXP=19937 \
          -I./csa -I./utility -I./shogi
LFLAGS  = -lpthread $(RTLIB)
endif
endif

HEADERS = shogi/Bitboard.h shogi/Color.h shogi/Convert.h shogi/Direction.h \
          shogi/Effect.h shogi/Evaluation.h shogi/Move.h \
          shogi/Piece.h shogi/Position.h shogi/Region.h shogi/Shogi.h \
          shogi/Square.h shogi/Zobrist.h \
          foundation/Common.h \
          foundation/Array.h foundation/Atomic.h foundation/BTree.h \
          foundation/HashTree.h foundation/List.h foundation/MaxHeap.h \
          foundation/MinHeap.h foundation/Semaphore.h \
          foundation/SpinLock.h foundation/Thread.h foundation/Vector.h \
          foundation/BitOperations.h utility/Utility.h \
          csa/CSASummary.h csa/CSAConnection.h csa/CSAFile.h

OBJS    = $(DSFMT)/dSFMT.o \
          utility/Utility.o csa/CSASummary.o csa/CSAConnection.o csa/CSAFile.o \
          shogi/Bitboard.o shogi/Direction.o shogi/Effect.o \
          shogi/Position.o shogi/Region.o shogi/Shogi.o \
          shogi/Square.o shogi/Zobrist.o

all: $(LIBNAME)

%.o: %.cpp $(HEADERS)
	$(CC) $(CFLAGS) -fPIC -c -o $@ $<

$(DSFMT)/dSFMT.o: $(DSFMT)/dSFMT.c $(DSFMT)/dSFMT.h
	CFLAGS='-DDSFMT_MEXP=19937' make -C dSFMT-src-2.2.3/ dSFMT.o

$(LIBNAME): $(HEADERS) $(OBJS)
	$(CC) $(SHAREDFLAGS) -o $@ $(OBJS) $(LFLAGS)

clean:
	rm -f shogi/*.o shogi/*.gch utility/*.o utility/*.gch
	rm -f csa/*.o csa/gch.* *.gch
	rm -f core.*
	rm -f $(LIBNAME)
	make -C $(DSFMT) clean

