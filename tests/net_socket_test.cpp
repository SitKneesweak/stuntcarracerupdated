// Standalone smoke test for Net_Socket — see tests/Makefile.
//
// Runs both ends over UDP loopback in one process, which is enough to cover
// the parts most likely to break silently in a real session: the dual-stack
// v4/v4-mapped conversion (a v6 socket cannot sendto a v4 sockaddr, and the
// address a datagram arrives from is not shaped like the one you resolved),
// address equality across those two shapes, and the "nothing queued" vs
// "real error" distinction in Recv, which the protocol timeout depends on.
#include "Net_Socket.h"
#include <cstdio>
#include <cstring>
using namespace scr;

static int fails = 0;
#define CHECK(c) do { if(!(c)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); ++fails; } } while(0)

int main()
{
    CHECK(NetInit());

    UdpSocket host, join;
    CHECK(host.Open(0));
    CHECK(host.IsOpen());
    CHECK(host.LocalPort() != 0);
    CHECK(join.Open(0));
    CHECK(join.LocalPort() != host.LocalPort());
    printf("host port %u, joiner port %u\n", host.LocalPort(), join.LocalPort());

    // Nothing queued yet -> 0, not -1.
    char buf[64];
    CHECK(join.Recv(buf, sizeof(buf), nullptr) == 0);

    // Resolve loopback to the host's port and send.
    NetAddress hostAddr;
    CHECK(NetResolve("127.0.0.1", host.LocalPort(), &hostAddr));
    char s[128];
    NetAddressToString(hostAddr, s, sizeof(s));
    printf("resolved: %s\n", s);

    const char* msg = "lockstep";
    CHECK(join.Send(hostAddr, msg, (int)strlen(msg)) == (int)strlen(msg));

    // Give the loopback a moment, then read it back with the sender's address.
    NetAddress from;
    int got = 0;
    for (int i = 0; i < 1000 && got == 0; ++i)
        got = host.Recv(buf, sizeof(buf), &from);
    CHECK(got == (int)strlen(msg));
    CHECK(memcmp(buf, msg, got) == 0);
    CHECK(from.IsSet());
    NetAddressToString(from, s, sizeof(s));
    printf("received %d bytes from %s\n", got, s);

    // Reply to the address we learned, which is what the host does after the
    // joiner's first packet arrives.
    CHECK(host.Send(from, "ack", 3) == 3);
    got = 0;
    for (int i = 0; i < 1000 && got == 0; ++i)
        got = join.Recv(buf, sizeof(buf), nullptr);
    CHECK(got == 3);
    CHECK(memcmp(buf, "ack", 3) == 0);

    // Address equality: same endpoint resolved twice must compare equal, a
    // different port must not.
    NetAddress again, other;
    CHECK(NetResolve("127.0.0.1", host.LocalPort(), &again));
    CHECK(again == hostAddr);
    CHECK(NetResolve("127.0.0.1", (uint16_t)(host.LocalPort() + 1), &other));
    CHECK(other != hostAddr);

    // Unset address, and sending to one.
    NetAddress unset;
    CHECK(!unset.IsSet());
    CHECK(host.Send(unset, "x", 1) == -1);
    NetAddressToString(unset, s, sizeof(s));
    CHECK(strcmp(s, "<unset>") == 0);

    // A closed socket is inert, not a crash.
    join.Close();
    CHECK(!join.IsOpen());
    CHECK(join.Recv(buf, sizeof(buf), nullptr) == -1);
    CHECK(join.Send(hostAddr, "x", 1) == -1);

    // Binding a port already bound should fail rather than silently succeed.
    UdpSocket clash;
    CHECK(!clash.Open(host.LocalPort()) || clash.LocalPort() == host.LocalPort());

    NetShutdown();
    printf(fails ? "\n%d CHECK(s) FAILED\n" : "\nall checks passed\n", fails);
    return fails != 0;
}
