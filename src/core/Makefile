CC = gcc
CXX = $(shell command -v ccache >/dev/null 2>&1 && echo "ccache g++" || echo "g++")
CFLAGS = -g -O0 -Wall -m64 -D__GUNC__ -fPIC
CPP_VERSION=$(shell g++ -dumpversion | awk '{if ($$NF > 5.0) print "c++14"; else print "c++11";}')
CXXFLAG = -std=$(CPP_VERSION) -g -O0 -Wall -Wno-unused-function -Wno-noexcept-type -m64 -D_GNU_SOURCE=1 -D_REENTRANT -D__GUNC__ -fPIC -DNODE_BEAT=10.0 -DTHREADED

VPATH = .
SUB_DIRS := $(foreach dir, $(VPATH), $(shell find $(dir) -maxdepth 5 -type d))
DIRS := $(SUB_DIRS)

INC := $(INC) \
       -I . \
	   -I /usr/local/include/mariadb

LDFLAGS := $(LDFLAGS) -D_LINUX_OS_ \
		  -L /usr/local/lib \
		  -L /usr/local/lib/mariadb \
          -L /usr/local/opt/openssl/lib \
          -lev -lprotobuf -lcryptopp -lhiredis -ljemalloc -ldl \
		  -lmariadb -lssl -lcrypto -lzookeeper_mt -lpthread

CPP_SRCS = $(foreach dir, $(DIRS), $(wildcard $(dir)/*.cpp))
CC_SRCS = $(foreach dir, $(DIRS), $(wildcard $(dir)/*.cc))
C_SRCS = $(foreach dir, $(DIRS), $(wildcard $(dir)/*.c))
OBJS = $(patsubst %.cpp,%.o,$(CPP_SRCS)) $(patsubst %.c,%.o,$(C_SRCS)) $(patsubst %.cc,%.o,$(CC_SRCS))

CCCOLOR="\033[34m"
LINKCOLOR="\033[34;1m"
SRCCOLOR="\033[33m"
BINCOLOR="\033[37;1m"
ENDCOLOR="\033[0m"
QUIET_CC = @printf '    %b %b\n' $(CCCOLOR)GCC$(ENDCOLOR) $(SRCCOLOR)$@$(ENDCOLOR) 1>&2;
QUIET_CPP = @printf '    %b %b\n' $(CCCOLOR)CXX$(ENDCOLOR) $(SRCCOLOR)$@$(ENDCOLOR) 1>&2;
QUIET_LINK = @printf '    %b %b\n' $(LINKCOLOR)LINK$(ENDCOLOR) $(BINCOLOR)$@$(ENDCOLOR) 1>&2;

SERVER_CC = $(QUIET_CC) $(CC) $(CFLAGS)
SERVER_LD = $(QUIET_LINK) $(CXX) $(CXXFLAG)
SERVER_CPP = $(QUIET_CPP) $(CXX) $(CXXFLAG)

SERVER_NAME = kimserver

.PHONY: clean

$(SERVER_NAME): $(OBJS)
	$(SERVER_LD) -o $@ $(OBJS) $(INC) $(LDFLAGS)
	cp -f $@ ../../bin/

%.o:%.cpp
	$(SERVER_CPP) $(INC) -c -o $@ $<
%.o:%.cc
	$(SERVER_CPP) $(INC) -c -o $@ $<
%.o:%.c
	$(SERVER_CC) $(INC)  -c -o $@ $<
clean:
	rm -f $(OBJS)
	rm -f $(SERVER_NAME)
	rm -f ../bin/$(SERVER_NAME)