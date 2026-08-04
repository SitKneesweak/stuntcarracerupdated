# Standalone tests. Not part of the game build — the game's Makefile globs
# *.cpp in the root only, so nothing here is linked into stuntcarracer.
#
#   make -C tests && ./tests/net_socket_test
#
# net_socket_test talks to itself over UDP loopback, so it needs no peer and no
# network, but a sandbox that blocks loopback sockets will fail it.

CXX ?= g++
CXXFLAGS ?= -std=gnu++14 -I.. -Wall -O1

ifeq ($(OS),Windows_NT)
    LIBS += -lws2_32
endif

all: net_socket_test

net_socket_test: net_socket_test.cpp ../Net_Socket.cpp ../Net_Socket.h
	$(CXX) $(CXXFLAGS) -o $@ net_socket_test.cpp ../Net_Socket.cpp $(LIBS)

check: net_socket_test
	./net_socket_test

clean:
	rm -f net_socket_test

.PHONY: all check clean
