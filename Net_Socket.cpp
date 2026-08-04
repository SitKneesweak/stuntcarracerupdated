// Net_Socket — see header. This is the ONLY translation unit in the game that
// includes a platform socket header, and it deliberately does not include
// dx_linux.h, dxstdafx.h or anything that reaches them. Keep it that way.

#include "Net_Socket.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
  // WIN32_LEAN_AND_MEAN keeps windows.h from dragging in winsock 1, which
  // would then conflict with winsock2 below.
  #define WIN32_LEAN_AND_MEAN
  #include <winsock2.h>
  #include <ws2tcpip.h>
  // SIO_UDP_CONNRESET (used in Bind) lives here, not in winsock2.h, on
  // mingw-w64 — winsock2.h alone compiles under MSVC and fails under MinGW.
  #include <mswsock.h>
  // GetAdaptersAddresses, for NetLocalAddresses. Must follow winsock2.h, and
  // brings -liphlpapi with it (see the MINGW branch of the Makefile).
  #include <iphlpapi.h>
  // Older mingw-w64 headers ship mswsock.h without it. It is a stable, publicly
  // documented control code, so spelling it out is safer than requiring a
  // particular header vintage.
  #ifndef SIO_UDP_CONNRESET
    #define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
  #endif
  typedef int socklen_t_compat;
  // The native handle type, so the intptr_t in the header is never narrowed at
  // a call site. Windows' SOCKET is UINT_PTR (64-bit on Win64); casting it
  // through int would corrupt any handle above 2^31.
  #define NET_FD(h)     ((SOCKET)(h))
  #define NET_INVALID   INVALID_SOCKET
  #define NET_LASTERROR WSAGetLastError()
  #define NET_EWOULDBLOCK WSAEWOULDBLOCK
#else
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <ifaddrs.h>
  #include <net/if.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <errno.h>
  typedef socklen_t socklen_t_compat;
  #define NET_FD(h)     ((int)(h))
  #define NET_INVALID   (-1)
  #define NET_LASTERROR errno
  #define NET_EWOULDBLOCK EWOULDBLOCK
#endif

namespace scr {

// The header's blob has to hold whatever the platform's largest address is.
// If this ever fails, grow NetAddress::storage — do not silently truncate.
static_assert(sizeof(struct sockaddr_storage) <= sizeof(((NetAddress*)0)->storage),
              "NetAddress::storage too small for sockaddr_storage");

namespace {

int   gInitRefs = 0;

inline sockaddr*       SA(NetAddress& a)       { return (sockaddr*)a.storage; }
inline const sockaddr* SA(const NetAddress& a) { return (const sockaddr*)a.storage; }

// Canonical form of an address: family-independent 16 bytes plus a port, with
// v4 promoted to its v4-mapped v6 form (::ffff:a.b.c.d).
//
// This exists because a dual-stack socket makes the same peer appear in two
// different shapes. NetResolve("127.0.0.1") hands back a sockaddr_in, but a
// datagram *received* from that same peer on a v6 socket arrives tagged
// AF_INET6 as ::ffff:127.0.0.1. Comparing the raw structs would call those two
// different peers, which in the protocol means silently ignoring the packets
// from the peer you just connected to.
struct CanonAddr
{
    uint8_t  addr[16];
    uint16_t port;
    uint32_t scope;
    bool     valid;
};

CanonAddr Canonicalise(const NetAddress& a)
{
    CanonAddr c;
    memset(&c, 0, sizeof(c));
    if (!a.IsSet())
        return c;

    const sockaddr* sa = SA(a);
    if (sa->sa_family == AF_INET)
    {
        const sockaddr_in* in4 = (const sockaddr_in*)sa;
        // ::ffff:0:0/96 — the v4-mapped prefix.
        c.addr[10] = 0xff;
        c.addr[11] = 0xff;
        memcpy(c.addr + 12, &in4->sin_addr, 4);
        c.port  = in4->sin_port;
        c.valid = true;
    }
    else if (sa->sa_family == AF_INET6)
    {
        const sockaddr_in6* in6 = (const sockaddr_in6*)sa;
        memcpy(c.addr, &in6->sin6_addr, 16);
        c.port  = in6->sin6_port;
        // Scope only disambiguates link-local addresses; for anything else it
        // is noise that would wrongly split one peer into two.
        if (c.addr[0] == 0xfe && (c.addr[1] & 0xc0) == 0x80)
            c.scope = in6->sin6_scope_id;
        c.valid = true;
    }
    return c;
}

bool IsV4Mapped(const uint8_t a[16])
{
    static const uint8_t prefix[12] = { 0,0,0,0, 0,0,0,0, 0,0,0xff,0xff };
    return memcmp(a, prefix, 12) == 0;
}

// Rewrite an address into the family a socket can actually send to. A v6
// socket needs v4 peers in v4-mapped form; a v4 socket can only reach a v6
// address if it is v4-mapped, and genuinely cannot reach anything else.
bool ToFamily(const NetAddress& in, int family, NetAddress* out)
{
    CanonAddr c = Canonicalise(in);
    if (!c.valid)
        return false;

    *out = NetAddress();
    if (family == AF_INET6)
    {
        sockaddr_in6 a;
        memset(&a, 0, sizeof(a));
        a.sin6_family = AF_INET6;
        a.sin6_port   = c.port;
        memcpy(&a.sin6_addr, c.addr, 16);
        a.sin6_scope_id = c.scope;
        memcpy(out->storage, &a, sizeof(a));
        out->len = sizeof(a);
        return true;
    }

    if (!IsV4Mapped(c.addr))
        return false;       // a real v6 peer, unreachable from a v4 socket

    sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port   = c.port;
    memcpy(&a.sin_addr, c.addr + 12, 4);
    memcpy(out->storage, &a, sizeof(a));
    out->len = sizeof(a);
    return true;
}

} // namespace

bool NetAddress::operator==(const NetAddress& o) const
{
    // Compare canonical forms, not the raw blobs. sockaddr_in has an 8-byte
    // sin_zero pad nothing is required to clear, sockaddr_in6 has a flowinfo
    // field that varies without changing which peer is meant, and — the one
    // that actually bites — the same peer legitimately appears as v4 in one
    // place and v4-mapped v6 in another. See Canonicalise.
    if (!IsSet() || !o.IsSet())
        return IsSet() == o.IsSet();

    CanonAddr x = Canonicalise(*this);
    CanonAddr y = Canonicalise(o);
    if (!x.valid || !y.valid)
        return len == o.len && memcmp(storage, o.storage, len) == 0;

    return x.port == y.port
        && x.scope == y.scope
        && memcmp(x.addr, y.addr, 16) == 0;
}

bool NetInit()
{
    if (gInitRefs++ > 0)
        return true;

#ifdef _WIN32
    WSADATA wsa;
    int rc = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (rc != 0)
    {
        printf("net: WSAStartup failed (%d)\n", rc);
        fflush(stdout);
        gInitRefs = 0;
        return false;
    }
#endif
    return true;
}

void NetShutdown()
{
    if (gInitRefs == 0)
        return;
    if (--gInitRefs > 0)
        return;

#ifdef _WIN32
    WSACleanup();
#endif
}

bool NetResolve(const char* host, uint16_t port, NetAddress* out)
{
    if (!out)
        return false;
    *out = NetAddress();

    char portStr[16];
    snprintf(portStr, sizeof(portStr), "%u", (unsigned)port);

    addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;      // v4 or v6, whichever the name gives
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    if (!host)
        hints.ai_flags = AI_PASSIVE;    // wildcard, for binding

    addrinfo* res = nullptr;
    int rc = getaddrinfo(host, portStr, &hints, &res);
    if (rc != 0 || !res)
    {
        printf("net: cannot resolve %s:%u (%s)\n",
               host ? host : "<any>", (unsigned)port, gai_strerror(rc));
        fflush(stdout);
        return false;
    }

    // Prefer the first result. getaddrinfo already orders these sensibly per
    // RFC 6724, so second-guessing it is how you end up unable to reach a
    // v6-only peer.
    memcpy(out->storage, res->ai_addr, res->ai_addrlen);
    out->len = (uint32_t)res->ai_addrlen;

    freeaddrinfo(res);
    return true;
}

void NetAddressToString(const NetAddress& addr, char* buf, int bufLen)
{
    if (!buf || bufLen <= 0)
        return;

    if (!addr.IsSet())
    {
        snprintf(buf, bufLen, "<unset>");
        return;
    }

    char host[NI_MAXHOST], serv[NI_MAXSERV];
    int rc = getnameinfo(SA(addr), (socklen_t_compat)addr.len,
                         host, sizeof(host), serv, sizeof(serv),
                         NI_NUMERICHOST | NI_NUMERICSERV);
    if (rc != 0)
    {
        snprintf(buf, bufLen, "<bad>");
        return;
    }

    // Bracket v6 so "::1:7777" doesn't read as ambiguous.
    if (SA(addr)->sa_family == AF_INET6)
        snprintf(buf, bufLen, "[%s]:%s", host, serv);
    else
        snprintf(buf, bufLen, "%s:%s", host, serv);
}

namespace {

// Append "text" to a comma-separated list already in buf, if it fits and is not
// there twice. Silently drops anything that would overflow - a truncated list
// is still readable, a truncated address is not.
void AppendUnique(char* buf, int bufLen, const char* text)
{
    const int have = (int)strlen(buf);
    const int want = (int)strlen(text);
    if (want == 0)
        return;

    // Substring match is enough: these are whole addresses drawn from the same
    // list, so a hit is the same address seen on a second interface.
    if (strstr(buf, text) != nullptr)
        return;

    const int sep = (have > 0) ? 2 : 0;      // ", "
    if (have + sep + want + 1 > bufLen)
        return;

    if (sep)
        strcpy(buf + have, ", ");
    strcpy(buf + have + sep, text);
}

// Worth reading out to the other player? Loopback only reaches this machine,
// link-local needs a scope suffix that the join screen has no way to carry, and
// the 169.254 autoconfiguration range means the DHCP lease never arrived.
bool IsUsefulV4(uint32_t hostOrder)
{
    if ((hostOrder >> 24) == 127)
        return false;
    if ((hostOrder >> 16) == 0xA9FE)        // 169.254/16
        return false;
    return hostOrder != 0;
}

bool IsUsefulV6(const struct in6_addr* a)
{
    const uint8_t* b = (const uint8_t*)a;
    if (b[0] == 0xFE && (b[1] & 0xC0) == 0x80)      // fe80::/10 link-local
        return false;
    for (int i = 0; i < 15; i++)                    // ::1 loopback
        if (b[i] != 0)
            return true;
    return b[15] != 1;
}

} // namespace

void NetLocalAddresses(char* buf, int bufLen)
{
    if (!buf || bufLen <= 0)
        return;
    buf[0] = '\0';

    // Two passes so every IPv4 address comes before any IPv6 one: v4 is what a
    // player can realistically read out loud, so it belongs at the front of a
    // line that may well be truncated.
#ifdef _WIN32
    // GetAdaptersAddresses wants a buffer it can grow into; 16K covers any
    // ordinary machine and the loop retries once if it does not.
    ULONG size = 16 * 1024;
    IP_ADAPTER_ADDRESSES* adapters = nullptr;
    for (int attempt = 0; attempt < 2; attempt++)
    {
        adapters = (IP_ADAPTER_ADDRESSES*)malloc(size);
        if (!adapters)
            return;
        const ULONG rc = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_SKIP_ANYCAST |
                                              GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
                                              nullptr, adapters, &size);
        if (rc == NO_ERROR)
            break;
        free(adapters);
        adapters = nullptr;
        if (rc != ERROR_BUFFER_OVERFLOW)
            break;
    }

    for (int family = 0; adapters && (family < 2); family++)
    {
        const int want = (family == 0) ? AF_INET : AF_INET6;
        for (IP_ADAPTER_ADDRESSES* a = adapters; a; a = a->Next)
        {
            if (a->OperStatus != IfOperStatusUp)
                continue;
            if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
                continue;

            for (IP_ADAPTER_UNICAST_ADDRESS* u = a->FirstUnicastAddress; u; u = u->Next)
            {
                sockaddr* sa = u->Address.lpSockaddr;
                if (!sa || sa->sa_family != want)
                    continue;
                if (want == AF_INET)
                {
                    if (!IsUsefulV4(ntohl(((sockaddr_in*)sa)->sin_addr.s_addr)))
                        continue;
                }
                else if (!IsUsefulV6(&((sockaddr_in6*)sa)->sin6_addr))
                    continue;

                char host[NI_MAXHOST];
                if (getnameinfo(sa, (socklen_t_compat)u->Address.iSockaddrLength,
                                host, sizeof(host), nullptr, 0, NI_NUMERICHOST) == 0)
                    AppendUnique(buf, bufLen, host);
            }
        }
    }
    free(adapters);
#else
    ifaddrs* list = nullptr;
    if (getifaddrs(&list) != 0)
        list = nullptr;

    for (int family = 0; list && (family < 2); family++)
    {
        const int want = (family == 0) ? AF_INET : AF_INET6;
        for (ifaddrs* a = list; a; a = a->ifa_next)
        {
            if (!a->ifa_addr || (a->ifa_addr->sa_family != want))
                continue;
            if (!(a->ifa_flags & IFF_UP) || (a->ifa_flags & IFF_LOOPBACK))
                continue;

            socklen_t_compat len;
            if (want == AF_INET)
            {
                if (!IsUsefulV4(ntohl(((sockaddr_in*)a->ifa_addr)->sin_addr.s_addr)))
                    continue;
                len = sizeof(sockaddr_in);
            }
            else
            {
                if (!IsUsefulV6(&((sockaddr_in6*)a->ifa_addr)->sin6_addr))
                    continue;
                len = sizeof(sockaddr_in6);
            }

            char host[NI_MAXHOST];
            if (getnameinfo(a->ifa_addr, len, host, sizeof(host), nullptr, 0,
                            NI_NUMERICHOST) == 0)
                AppendUnique(buf, bufLen, host);
        }
    }

    if (list)
        freeifaddrs(list);
#endif

    if (buf[0] == '\0')
        snprintf(buf, bufLen, "unknown");
}

UdpSocket::UdpSocket()
    : mHandle(NET_INVALID), mPort(0), mFamily(0)
{
}

UdpSocket::~UdpSocket()
{
    Close();
}

bool UdpSocket::IsOpen() const
{
    return mHandle != (intptr_t)NET_INVALID;
}

uint16_t UdpSocket::LocalPort() const
{
    return mPort;
}

bool UdpSocket::Open(uint16_t localPort)
{
    Close();

    // Try v6 first with V6ONLY off, which gets both families on one socket.
    // A v6-less host (or a stack that refuses the dual-stack option) falls
    // back to v4 — direct-IP play over v4 is the common case and must not be
    // held hostage to v6 availability.
    int family = AF_INET6;
    intptr_t s = (intptr_t)socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if (s == (intptr_t)NET_INVALID)
    {
        family = AF_INET;
        s = (intptr_t)socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (s == (intptr_t)NET_INVALID)
        {
            printf("net: socket() failed (%d)\n", NET_LASTERROR);
            fflush(stdout);
            return false;
        }
    }
    else
    {
        int off = 0;
        if (setsockopt(NET_FD(s), IPPROTO_IPV6, IPV6_V6ONLY,
                       (const char*)&off, sizeof(off)) != 0)
        {
            // Dual-stack refused. A v6-only socket cannot hear a v4 peer, so
            // drop back to v4 rather than appear to work and then never
            // receive anything.
#ifdef _WIN32
            closesocket(NET_FD(s));
#else
            close(NET_FD(s));
#endif
            family = AF_INET;
            s = (intptr_t)socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (s == (intptr_t)NET_INVALID)
            {
                printf("net: socket() failed (%d)\n", NET_LASTERROR);
                fflush(stdout);
                return false;
            }
        }
    }

    mHandle = s;
    mFamily = family;

    // Bind.
    sockaddr_storage ss;
    memset(&ss, 0, sizeof(ss));
    socklen_t_compat sslen;
    if (family == AF_INET6)
    {
        sockaddr_in6* a = (sockaddr_in6*)&ss;
        a->sin6_family = AF_INET6;
        a->sin6_addr   = in6addr_any;
        a->sin6_port   = htons(localPort);
        sslen = sizeof(sockaddr_in6);
    }
    else
    {
        sockaddr_in* a = (sockaddr_in*)&ss;
        a->sin_family      = AF_INET;
        a->sin_addr.s_addr = htonl(INADDR_ANY);
        a->sin_port        = htons(localPort);
        sslen = sizeof(sockaddr_in);
    }

    if (bind(NET_FD(mHandle), (sockaddr*)&ss, sslen) != 0)
    {
        printf("net: cannot bind port %u (%d)\n",
               (unsigned)localPort, NET_LASTERROR);
        fflush(stdout);
        Close();
        return false;
    }

    // Read the port back — after Open(0) it is the only way to learn it, and
    // the host UI has to display it.
    sockaddr_storage bound;
    socklen_t_compat boundLen = sizeof(bound);
    if (getsockname(NET_FD(mHandle), (sockaddr*)&bound, &boundLen) == 0)
    {
        if (bound.ss_family == AF_INET6)
            mPort = ntohs(((sockaddr_in6*)&bound)->sin6_port);
        else
            mPort = ntohs(((sockaddr_in*)&bound)->sin_port);
    }
    else
    {
        mPort = localPort;
    }

    // Non-blocking. The frame loop drains the socket each tick and must never
    // stall on it.
#ifdef _WIN32
    u_long nb = 1;
    if (ioctlsocket(NET_FD(mHandle), FIONBIO, &nb) != 0)
#else
    int flags = fcntl(NET_FD(mHandle), F_GETFL, 0);
    if (flags < 0 || fcntl(NET_FD(mHandle), F_SETFL, flags | O_NONBLOCK) != 0)
#endif
    {
        printf("net: cannot set non-blocking (%d)\n", NET_LASTERROR);
        fflush(stdout);
        Close();
        return false;
    }

#ifdef _WIN32
    // Winsock surfaces an ICMP port-unreachable from a *previous* send as a
    // WSAECONNRESET on the next recvfrom, which for UDP is worse than useless:
    // it makes an ordinary "peer not listening yet" look like a dead socket.
    // SIO_UDP_CONNRESET off restores the POSIX behaviour of ignoring it.
    {
        DWORD off = 0, ret = 0;
        WSAIoctl(NET_FD(mHandle), SIO_UDP_CONNRESET, &off, sizeof(off),
                 nullptr, 0, &ret, nullptr, nullptr);
    }
#endif

    return true;
}

void UdpSocket::Close()
{
    if (!IsOpen())
        return;

#ifdef _WIN32
    closesocket(NET_FD(mHandle));
#else
    close(NET_FD(mHandle));
#endif
    mHandle = NET_INVALID;
    mPort   = 0;
    mFamily = 0;
}

int UdpSocket::Send(const NetAddress& to, const void* data, int len)
{
    if (!IsOpen() || !to.IsSet() || len <= 0)
        return -1;

    // The caller's address may be in the other family's shape — a v4 literal
    // from NetResolve while this socket is dual-stack v6, most commonly. A
    // sendto with a mismatched family fails outright, so convert first.
    NetAddress dest;
    if (!ToFamily(to, mFamily, &dest))
        return -1;

    int sent = (int)sendto(NET_FD(mHandle), (const char*)data, len, 0,
                           SA(dest), (socklen_t_compat)dest.len);
    return sent;
}

int UdpSocket::Recv(void* buf, int len, NetAddress* from)
{
    if (!IsOpen() || len <= 0)
        return -1;

    sockaddr_storage ss;
    socklen_t_compat sslen = sizeof(ss);
    int got = (int)recvfrom(NET_FD(mHandle), (char*)buf, len, 0,
                            (sockaddr*)&ss, &sslen);
    if (got < 0)
    {
        int err = NET_LASTERROR;
        if (err == NET_EWOULDBLOCK)
            return 0;               // nothing queued; the normal answer
#ifndef _WIN32
        if (err == EAGAIN || err == EINTR)
            return 0;
#endif
        return -1;
    }

    if (from)
    {
        *from = NetAddress();
        if (sslen > 0 && (size_t)sslen <= sizeof(from->storage))
        {
            memcpy(from->storage, &ss, sslen);
            from->len = (uint32_t)sslen;
        }
    }
    return got;
}

} // namespace scr
