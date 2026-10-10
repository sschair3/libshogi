
PREFIX  = /usr/local
INCDIR  = $(PREFIX)/include/shogi
ifeq ($(shell uname -s),Darwin)
LIBDIR  = $(PREFIX)/lib
else
LIBDIR  = $(PREFIX)/lib64
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
          -O0 -g $(ARCHFLAGS) -ftrapv -fthreadsafe-statics \
          -fsanitize=address \
		  -I$(INCDIR) -I./lesserpyon -D_GCC_
LFLAGS  =  -pthread $(RTLIB) -L$(LIBDIR) -lshogi
else
CFLAGS += -std=c++14 -Wall -Wextra -Wformat=2 -Wstrict-aliasing=2 \
          -Wcast-qual -Wcast-align -Wwrite-strings -Wconversion \
          -Wfloat-equal -Wpointer-arith -Wswitch-enum \
          -O0 -g $(ARCHFLAGS) -fthreadsafe-statics \
		  -I$(INCDIR) -I./lesserpyon -D_GCC_
LFLAGS  = -pthread $(RTLIB) -L$(LIBDIR) -lshogi
endif
else
ifeq ($(MCHCK),y)
CFLAGS += -std=c++14 -Wall -Wextra -Wformat=2 -Wstrict-aliasing=2 \
          -Wcast-qual -Wcast-align -Wwrite-strings -Wconversion \
          -Wfloat-equal -Wpointer-arith -Wswitch-enum \
		  -O3 -g $(ARCHFLAGS) -funroll-loops \
          -fthreadsafe-statics -ftrapv -fsanitize=address \
		  -I$(INCDIR) -I./lesserpyon -D_GCC_
LFLAGS  = -pthread $(RTLIB) -L$(LIBDIR) -lshogi
else
CFLAGS += -std=c++14 -Wall -Wextra -Wformat=2 -Wstrict-aliasing=2 \
          -Wcast-qual -Wcast-align -Wwrite-strings -Wconversion \
          -Wfloat-equal -Wpointer-arith -Wswitch-enum \
		  -O3 -g $(ARCHFLAGS) -funroll-loops \
          -fthreadsafe-statics \
		  -I$(INCDIR) -I./lesserpyon -D_GCC_
LFLAGS  = -lpthread $(RTLIB) -L$(LIBDIR) -lshogi
endif
endif

HEADERS = MyPosition.h
OBJS    = MyPosition.o 
LESPYN  = lesserpyon/Joseki.o lesserpyon/KomaMoves.o \
          lesserpyon/Te.o lesserpyon/kyokumen.o
EXECS   = testeffect testpin testcheck testvalue testgetout \
          testmove testfast testcapt testoute testofst \
          testhash testundo movebench

all: $(EXECS)

%.o: %.cpp $(HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<

testeffect: TestEffect.o $(HEADERS) $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LFLAGS)

testpin: TestPin.o $(HEADERS) $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LFLAGS)

testcheck: TestCheck.o $(HEADERS) $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LFLAGS)

testvalue: TestValue.o $(HEADERS) $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LFLAGS)

testgetout: TestGetout.o $(HEADERS) $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LFLAGS)

testmove: TestMove.o $(HEADERS) $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LESPYN) $(LFLAGS)

testfast: TestFast.o $(HEADERS) $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LESPYN) $(LFLAGS)

testcapt: TestCapt.o $(HEADERS) $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LESPYN) $(LFLAGS)

testoute: TestOute.o $(HEADERS) $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LESPYN) $(LFLAGS)

testofst: TestOFst.o $(HEADERS) $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LESPYN) $(LFLAGS)

testhash: TestHash.o $(HEADERS) $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LFLAGS)

testundo: TestUndo.o $(HEADERS) $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LFLAGS)

movebench: MoveBench.o $(HEADERS) $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LFLAGS)

clean:
	rm -f *.o $(EXECS)
