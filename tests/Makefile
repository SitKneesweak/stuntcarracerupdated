# Standalone tests. Not part of the game build — the game's Makefile globs
# *.cpp in the root only, so nothing here is linked into stuntcarracer.
#
#   make -C tests && ./tests/net_socket_test
#
# Both tests talk to themselves over UDP loopback, so they need no peer and no
# network, but a sandbox that blocks loopback sockets will fail them.
# net_lockstep_test forks a second process for the joining peer and so is POSIX
# only; it binds port 47921.

CXX ?= g++
CXXFLAGS ?= -std=gnu++14 -I.. -Wall -O1

ifeq ($(OS),Windows_NT)
    LIBS += -lws2_32
endif

TESTS = net_socket_test profile_test physics_rate_test opponent_speed_test
ifneq ($(OS),Windows_NT)
    # needs fork()
    TESTS += net_lockstep_test
endif

all: $(TESTS)

profile_test: profile_test.cpp ../Profile.cpp ../League.cpp ../Profile.h ../League.h
	$(CXX) $(CXXFLAGS) -o $@ profile_test.cpp ../Profile.cpp ../League.cpp

# physics_rate_test needs the same -ffp-contract=off the game builds the
# physics with, or the compiler fuses multiply-adds differently between
# translation units and the "identical" runs stop being identical.
physics_rate_test: physics_rate_test.cpp ../Physics_FloatV2.cpp ../Track_FloatV2.cpp \
                   ../Physics_FloatV2.h ../Track_FloatV2.h ../Det_Math.h
	$(CXX) $(CXXFLAGS) -ffp-contract=off -o $@ physics_rate_test.cpp \
	    ../Physics_FloatV2.cpp ../Track_FloatV2.cpp

# Header-only: Opponent_Speeds.h deliberately has no game or platform dependency
# so the test can run the same generator the game runs.  Reads ../Tracks/*.bin.
opponent_speed_test: opponent_speed_test.cpp ../Opponent_Speeds.h
	$(CXX) $(CXXFLAGS) -o $@ opponent_speed_test.cpp

net_socket_test: net_socket_test.cpp ../Net_Socket.cpp ../Net_Socket.h
	$(CXX) $(CXXFLAGS) -o $@ net_socket_test.cpp ../Net_Socket.cpp $(LIBS)

net_lockstep_test: net_lockstep_test.cpp ../Net_Lockstep.cpp ../Net_Socket.cpp ../Net_Lockstep.h ../Net_Socket.h
	$(CXX) $(CXXFLAGS) -o $@ net_lockstep_test.cpp ../Net_Lockstep.cpp ../Net_Socket.cpp $(LIBS)

check: $(TESTS)
	@for t in $(TESTS); do echo "=== $$t"; ./$$t || exit 1; done
ifneq ($(OS),Windows_NT)
	@echo "=== net_lockstep_test desync"; ./net_lockstep_test desync || exit 1
endif

clean:
	rm -f $(TESTS)

.PHONY: all check clean
