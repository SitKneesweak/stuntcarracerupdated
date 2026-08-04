// Net_Socket — a minimal UDP socket, portable across Linux, macOS and Windows.
//
// THIS HEADER MUST NOT INCLUDE ANY PLATFORM HEADER, and neither must anything
// that includes it. Winsock collides with dx_linux.h exactly as windows.h does:
// dx_linux.h typedefs DWORD, BOOL, WCHAR, HRESULT and HWND itself with
// *different underlying types* than Win32 (BOOL as uint32_t vs int; HWND as
// uint32_t vs a pointer). Since the game's translation units are full of
// dx_linux.h, the only safe arrangement is for <winsock2.h>/<sys/socket.h> to
// appear in Net_Socket.cpp and nowhere else. Hence:
//
//   - the socket handle is an intptr_t, not a SOCKET or an int. Windows'
//     SOCKET is UINT_PTR, so it is 64-bit on Win64 and an int would truncate
//     it; POSIX fds are ints and widen harmlessly.
//   - an address is an opaque byte blob big enough for sockaddr_storage,
//     rather than a sockaddr, so no platform struct is named here.
//
// UDP only, and non-blocking only — that is all the lockstep transport wants.
// See Net_Lockstep.h for the protocol that runs over this.

#ifndef NET_SOCKET_H
#define NET_SOCKET_H

#include <cstdint>

namespace scr {

// Opaque address. Large enough for sockaddr_storage (128 bytes on every target
// we build for) with room to spare; Net_Socket.cpp static_asserts the fit, so a
// platform where that is wrong fails at compile time rather than silently
// truncating an address.
struct NetAddress
{
    uint8_t  storage[128];
    uint32_t len;       // 0 means "unset"

    NetAddress() : storage{}, len(0) {}
    bool IsSet() const { return len != 0; }
    bool operator==(const NetAddress& o) const;
    bool operator!=(const NetAddress& o) const { return !(*this == o); }
};

// Process-wide setup. No-op on POSIX; WSAStartup/WSACleanup on Windows.
// Safe to call repeatedly — refcounted.
bool NetInit();
void NetShutdown();

// Resolve "host" (a name or a literal v4/v6 address) plus port into an address.
// Blocking — a hostname lookup can stall for seconds, so call it from the
// connect UI, never from the frame loop. Pass nullptr for host to get the
// wildcard address for binding.
bool NetResolve(const char* host, uint16_t port, NetAddress* out);

// Human-readable "addr:port", for logs and the connect UI. Never fails; writes
// "<unset>" or "<bad>" rather than leaving the buffer untouched.
void NetAddressToString(const NetAddress& addr, char* buf, int bufLen);

// A bound, non-blocking UDP socket.
class UdpSocket
{
public:
    UdpSocket();
    ~UdpSocket();

    // Bind to localPort (0 = let the OS choose, which is what the joining side
    // wants). Dual-stack: binds v6 with IPV6_V6ONLY off where available, so one
    // socket serves both families, and falls back to v4 if v6 is unavailable.
    bool Open(uint16_t localPort);
    void Close();
    bool IsOpen() const;

    // The port actually bound, which is only interesting after Open(0).
    uint16_t LocalPort() const;

    // Returns bytes sent, or -1. A -1 from UDP is routine (ICMP unreachable
    // from a previous datagram surfaces here on some stacks) and is never
    // grounds for tearing the session down — let the protocol's own timeout
    // decide that.
    int Send(const NetAddress& to, const void* data, int len);

    // Returns bytes received, 0 if nothing is queued, -1 on a real error.
    // Non-blocking, so 0 is the overwhelmingly common answer.
    int Recv(void* buf, int len, NetAddress* from);

private:
    intptr_t mHandle;   // SOCKET or fd; -1 when closed
    uint16_t mPort;
    int      mFamily;   // AF_INET or AF_INET6, as actually bound. Send()
                        // rewrites outgoing addresses into this family.

    UdpSocket(const UdpSocket&);
    UdpSocket& operator=(const UdpSocket&);
};

} // namespace scr

#endif // NET_SOCKET_H
