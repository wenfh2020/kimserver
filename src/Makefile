CC = gcc
CXX = $(shell command -v ccache >/dev/null 2>&1 && echo "ccache g++" || echo "g++")
CFLAGS = -g -O0 -Wall -m64 -D__GUNC__ -fPIC
cplusplus_version=$(shell g++ -dumpversion | awk '{if ($$NF > 5.0) print "c++14"; else print "c++11";}')
CXXFLAG = -std=$(cplusplus_version) -g -O0 -Wall -m64 -D_GNU_SOURCE=1 -D_REENTRANT -D__GUNC__ -fPIC -DNODE_BEAT=10.0

VPATH = .
SUB_DIRS := $(foreach dir, $(VPATH), $(shell find $(dir) -maxdepth 5 -type d))
DIRS := $(SUB_DIRS)


INC := $(INC) \
       -I .

LDFLAGS := $(LDFLAGS) -D_LINUX_OS_ \
          -lev \

CPP_SRCS = $(foreach dir, $(DIRS), $(wildcard $(dir)/*.cpp))
CC_SRCS = $(foreach dir, $(DIRS), $(wildcard $(dir)/*.cc))
C_SRCS = $(foreach dir, $(DIRS), $(wildcard $(dir)/*.c))
OBJS = $(patsubst %.cpp,%.o,$(CPP_SRCS)) $(patsubst %.c,%.o,$(C_SRCS)) $(patsubst %.cc,%.o,$(CC_SRCS))

TARGET = kimserver

.PHONY: clean

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAG) -o $@ $(OBJS) $(INC) $(LDFLAGS)
	cp -f $@ ../bin/

%.o:%.cpp
	$(CXX) $(INC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $< 
%.o:%.cc
	$(CXX) $(INC) $(CPPFLAGS) -c -o $@ $<
%.o:%.c
	$(CC) $(INC) $(CFLAGS) -c -o $@ $<
clean:
	rm -f $(OBJS)
	rm -f $(TARGET)
	rm -f ../bin/$(TARGET)