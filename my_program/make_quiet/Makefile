FILE_NAME := quiet

# エンジンごとに使うファイル
#   make                    -> engine  (Search.cpp, 評価は駒割りだけ)
#   make FILE_NAME=engine2  -> engine2 (Search2.cpp + eval2.cpp, pst2.txt を読む)
#   make FILE_NAME=engine3  -> engine3 (Search2.cpp + eval3.cpp, pst3.txt を読む)
# HEADERS   = Search.h eval.h
# OBJS      = Search.o eval.o


PREFIX  = /usr/local
INCDIR  = $(PREFIX)/include/shogi
ifeq ($(shell uname -s),Darwin)
LIBDIR  = $(PREFIX)/lib
else
LIBDIR  = $(PREFIX)/lib64
endif

EXEC    = $(FILE_NAME)

DEBUG   = n
MCHCK   = n
BMI2    = n
TLS     = n


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

CC      = $(CXX)

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
		  -I$(INCDIR)
LFLAGS  =  -pthread $(RTLIB) -L$(LIBDIR) -lshogi
else
CFLAGS += -std=c++14 -Wall -Wextra -Wformat=2 -Wstrict-aliasing=2 \
          -Wcast-qual -Wcast-align -Wwrite-strings -Wconversion \
          -Wfloat-equal -Wpointer-arith -Wswitch-enum \
          -O0 -g $(ARCHFLAGS) -fthreadsafe-statics \
		  -I$(INCDIR)
LFLAGS  = -pthread $(RTLIB) -L$(LIBDIR) -lshogi
endif
else
ifeq ($(MCHCK),y)
CFLAGS += -std=c++14 -Wall -Wextra -Wformat=2 -Wstrict-aliasing=2 \
          -Wcast-qual -Wcast-align -Wwrite-strings -Wconversion \
          -Wfloat-equal -Wpointer-arith -Wswitch-enum \
		  -O3 -g $(ARCHFLAGS) -funroll-loops \
          -fthreadsafe-statics -ftrapv -fsanitize=address \
		  -I$(INCDIR)
LFLAGS  = -pthread $(RTLIB) -L$(LIBDIR) -lshogi
else
CFLAGS += -std=c++14 -Wall -Wextra -Wformat=2 -Wstrict-aliasing=2 \
          -Wcast-qual -Wcast-align -Wwrite-strings -Wconversion \
          -Wfloat-equal -Wpointer-arith -Wswitch-enum \
		  -O3 -g $(ARCHFLAGS) -funroll-loops \
          -fthreadsafe-statics \
		  -I$(INCDIR)
LFLAGS  = -lpthread $(RTLIB) -L$(LIBDIR) -lshogi
endif
endif

all: $(EXEC)

%.o: %.cpp $(HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<

$(EXEC): $(FILE_NAME).o $(HEADERS) $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LFLAGS)

clean:
	rm -f *.o $(EXEC)
