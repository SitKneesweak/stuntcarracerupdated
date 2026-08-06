// End-to-end test for Net_Lockstep — see tests/Makefile.
//
// The session is a file-scope global (the game only ever has one), so the two
// peers cannot both live in this process. The test forks: the parent hosts, the
// child joins, and they play a scripted race against each other over loopback.
//
// What this is really checking is that the two sides stay in lockstep — that
// every step is simulated with the same pair of inputs on both machines. Each
// peer folds the input pair for every step into a rolling hash and the two are
// compared at the end. If the protocol ever delivered an input to the wrong
// step, or dropped one, or let a step through before both were known, that
// hash diverges.
//
// POSIX only, for fork(). The protocol code itself is portable; this harness
// is not, and the Makefile skips it on Windows.

#include "../Net_Lockstep.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#endif

using namespace scr;

static int fails = 0;
#define CHECK(c) do { if(!(c)) { printf("[%s] FAIL %s:%d  %s\n", gWho, __FILE__, __LINE__, #c); ++fails; } } while(0)

static const char* gWho = "?";
static const uint16_t kPort = 47921;
static const uint32_t kSteps = 400;

// When set, the joiner deliberately reports a wrong digest from this step on,
// standing in for a genuine physics divergence (track 7 is a real one). The
// host must notice and say so rather than drift on silently.
static bool     gForceDesync = false;
static uint32_t gDesyncFrom  = 60;

#ifndef _WIN32

static double Now()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static void SleepMs(int ms)
{
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, nullptr);
}

// A scripted input stream, different for each side so a bug that crosses the
// two over would show up rather than cancel out.
static uint32_t ScriptedInput(uint32_t step, bool host)
{
    uint32_t r = step * 1664525u + (host ? 1013904223u : 22695477u);
    r ^= r >> 16;
    return r & 0x1f;        // the five KEY_P1_* bits
}

// Rolling hash of (step, localInput, remoteInput) over the whole race. Both
// peers must arrive at the same value — but note each peer's "local" is the
// other's "remote", so fold them in a role-independent order.
static uint64_t Fold(uint64_t h, uint32_t step, uint32_t hostIn, uint32_t joinIn)
{
    h ^= step;          h *= 1099511628211ull;
    h ^= hostIn;        h *= 1099511628211ull;
    h ^= joinIn;        h *= 1099511628211ull;
    return h;
}

// Drive one side of the race to completion. Returns the agreement hash, or 0
// if the run did not finish.
static uint64_t RunPeer(bool host, uint32_t* stepsDone, int* maxStallFrames)
{
    // Wait for the handshake.
    double start = Now();
    while (NetGetState() != NetState_Connected)
    {
        NetPoll(Now());
        if (NetGetState() == NetState_Failed)
        {
            printf("[%s] handshake failed: %s\n", gWho,
                   NetFailReasonText(NetGetFailReason()));
            return 0;
        }
        if (Now() - start > 15.0)
        {
            printf("[%s] handshake timed out\n", gWho);
            return 0;
        }
        SleepMs(1);
    }

    CHECK(NetGetConfig().track == 3);
    CHECK(NetGetConfig().seed  == 0x12345678u);
    CHECK(NetGetConfig().dt    == 1.0 / 60.0);

    // --- Control channel -----------------------------------------------
    // The between-races channel: the driver's name and the host's choice of
    // the next circuit ride on this. Exercised here, before the race, because
    // that is when the game uses it - and because a message that arrived twice
    // would score a race twice.
    {
        // Each end sends two, to prove the second only goes once the first has
        // been acknowledged and that neither is delivered more than once.
        const uint8_t first[3]  = { 1, host ? (uint8_t)'H' : (uint8_t)'J', 7 };
        const uint8_t second[2] = { 2, host ? (uint8_t)5   : (uint8_t)6 };

        CHECK(NetControlIdle());
        CHECK(NetSendControl(first, sizeof(first), Now()));
        CHECK(!NetControlIdle());
        CHECK(!NetSendControl(second, sizeof(second), Now()));   // one at a time

        int  got     = 0;
        bool sentTwo = false;
        double cstart = Now();
        while ((got < 2) && (Now() - cstart < 10.0))
        {
            NetPoll(Now());

            if (!sentTwo && NetControlIdle())
                sentTwo = NetSendControl(second, sizeof(second), Now());

            uint8_t in[kMaxControlBytes];
            const int n = NetReceiveControl(in, sizeof(in));
            if (n > 0)
            {
                const uint8_t peerTag = host ? (uint8_t)'J' : (uint8_t)'H';
                if (got == 0)
                {
                    CHECK(n == 3);
                    CHECK(in[0] == 1 && in[1] == peerTag && in[2] == 7);
                }
                else
                {
                    CHECK(n == 2);
                    CHECK(in[0] == 2 && in[1] == (host ? 6 : 5));
                }
                ++got;
            }
            SleepMs(1);
        }

        CHECK(got == 2);            // both arrived
        CHECK(sentTwo);             // and the second one got out

        // Nothing left over: a resend that was already delivered must not
        // surface again.
        double drain = Now();
        while (Now() - drain < 0.5)
        {
            NetPoll(Now());
            uint8_t in[kMaxControlBytes];
            CHECK(NetReceiveControl(in, sizeof(in)) == 0);
            SleepMs(1);
        }
    }

    uint64_t h = 1469598103934665603ull;
    uint32_t step = 0;
    int stall = 0, worstStall = 0;

    start = Now();
    while (step < kSteps)
    {
        NetPoll(Now());

        if (NetGetState() == NetState_Desynced)
        {
            printf("[%s] desync reported at step %u\n", gWho, NetDesyncStep());
            *stepsDone = step;
            if (!gForceDesync)
                return 0;

            // Detection is symmetric -- each peer compares the digest it
            // receives against its own, so both sides find it independently
            // within a digest interval. Stay up so the other peer gets there
            // too; quitting immediately would hand it a Bye instead and the
            // test would prove nothing about its detection.
            double d = Now();
            while (Now() - d < 2.0)
            {
                NetPoll(Now());
                SleepMs(1);
            }
            return 1;
        }
        if (NetGetState() != NetState_Connected && NetGetState() != NetState_Stalled)
        {
            printf("[%s] session ended early in state %d\n", gWho, (int)NetGetState());
            return 0;
        }
        if (Now() - start > 30.0)
        {
            printf("[%s] race timed out at step %u\n", gWho, step);
            return 0;
        }

        // Schedule this step's input, then see whether the step can run.
        NetSubmitInput(step, ScriptedInput(step, host));

        if (!NetStepReady(step))
        {
            ++stall;
            if (stall > worstStall) worstStall = stall;
            SleepMs(1);
            continue;
        }
        stall = 0;

        uint32_t mine  = NetLocalInput(step);
        uint32_t their = NetRemoteInput(step);

        // What step S consumes is what was pressed at step S - kInputDelay --
        // that is what the input delay *is*, not an off-by-one. The first
        // kInputDelay steps have no author at all and are the neutral window
        // both peers prime identically.
        uint32_t expectMine  = (step < (uint32_t)kInputDelay)
                             ? 0u : ScriptedInput(step - kInputDelay, host);
        uint32_t expectTheir = (step < (uint32_t)kInputDelay)
                             ? 0u : ScriptedInput(step - kInputDelay, !host);

        // This is the check that catches an input landing on the wrong step.
        CHECK(mine  == expectMine);
        CHECK(their == expectTheir);

        uint32_t hostIn = host ? mine  : their;
        uint32_t joinIn = host ? their : mine;
        h = Fold(h, step, hostIn, joinIn);

        // Normally a digest both sides agree on, so the detector stays quiet.
        // In desync mode the joiner corrupts it, as a diverged sim would.
        uint64_t reported = h;
        if (gForceDesync && !host && step >= gDesyncFrom)
            reported ^= 0xdeadbeefull;
        NetReportDigest(step, reported);

        ++step;
    }

    *stepsDone      = step;
    *maxStallFrames = worstStall;

    // Linger before returning. Both peers run the same number of steps but not
    // in the same instant, so whoever finishes first would otherwise close the
    // session -- and its Bye would drop the other peer mid-race. Keep pumping
    // so the straggler can finish.
    double lingerStart = Now();
    while (Now() - lingerStart < 1.0)
    {
        NetPoll(Now());
        SleepMs(1);
    }

    return h;
}

int main(int argc, char** argv)
{
    gForceDesync = (argc > 1 && !strcmp(argv[1], "desync"));
    if (gForceDesync)
        printf("=== desync mode: the joiner will report a corrupted digest "
               "from step %u\n", gDesyncFrom);

    // Both sides agree these; the host announces them and the joiner adopts.
    NetSessionConfig cfg;
    cfg.dt       = 1.0 / 60.0;
    cfg.seed     = 0x12345678u;
    cfg.track    = 3;
    cfg.opponent = 1;

    int pipefd[2];
    if (pipe(pipefd) != 0)
    {
        printf("pipe() failed\n");
        return 1;
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        printf("fork() failed\n");
        return 1;
    }

    if (pid == 0)
    {
        // --- Child: the joiner -------------------------------------------
        gWho = "joiner";
        close(pipefd[0]);
        SleepMs(150);           // let the host bind first

        if (!NetJoin("127.0.0.1", kPort, Now()))
        {
            printf("[joiner] NetJoin failed\n");
            _exit(1);
        }
        CHECK(NetGetRole() == NetRole_Joiner);

        uint32_t steps = 0;
        int stalls = 0;
        uint64_t h = RunPeer(false, &steps, &stalls);
        printf("[joiner] %u steps, worst stall %d polls, rtt %.1fms, hash %016llx\n",
               steps, stalls, NetGetRTT() * 1000.0, (unsigned long long)h);

        fflush(stdout);
        ssize_t ignored = write(pipefd[1], &h, sizeof(h));
        (void)ignored;
        close(pipefd[1]);

        NetClose();
        _exit(fails != 0 ? 1 : 0);
    }

    // --- Parent: the host ------------------------------------------------
    gWho = "host";
    close(pipefd[1]);

    if (!NetHost(kPort, cfg, Now()))
    {
        printf("[host] NetHost failed — is port %u in use?\n", kPort);
        kill(pid, SIGKILL);
        waitpid(pid, nullptr, 0);
        return 1;
    }
    CHECK(NetGetRole() == NetRole_Host);
    CHECK(NetGetState() == NetState_Listening);

    uint32_t steps = 0;
    int stalls = 0;
    uint64_t hostHash = RunPeer(true, &steps, &stalls);
    printf("[host]   %u steps, worst stall %d polls, rtt %.1fms, hash %016llx\n",
           steps, stalls, NetGetRTT() * 1000.0, (unsigned long long)hostHash);

    uint64_t joinHash = 0;
    ssize_t got = read(pipefd[0], &joinHash, sizeof(joinHash));
    close(pipefd[0]);

    int childStatus = 0;
    waitpid(pid, &childStatus, 0);

    CHECK(got == (ssize_t)sizeof(joinHash));

    if (gForceDesync)
    {
        // The host must have noticed, and must have noticed at a digest
        // checkpoint at or after the step the joiner started lying.
        CHECK(NetHasDesync());
        CHECK(NetGetState() == NetState_Desynced);
        CHECK(NetDesyncStep() >= gDesyncFrom);
        CHECK(NetGetFailReason() == NetFail_Desync);
        CHECK(steps < kSteps);          // the race stopped rather than drifting on
        printf("[host]   desync correctly detected at step %u\n", NetDesyncStep());
    }
    else
    {
        CHECK(steps == kSteps);
        CHECK(hostHash != 0);
        CHECK(!NetHasDesync());

        // The whole point: both machines simulated every step with the same
        // pair of inputs.
        CHECK(hostHash == joinHash);
        if (hostHash != joinHash)
            printf("host %016llx != joiner %016llx\n",
                   (unsigned long long)hostHash, (unsigned long long)joinHash);
    }

    CHECK(WIFEXITED(childStatus) && WEXITSTATUS(childStatus) == 0);

    NetClose();
    CHECK(NetGetState() == NetState_Idle);

    printf(fails ? "\n%d CHECK(s) FAILED\n" : "\nall checks passed\n", fails);
    return fails != 0;
}

#else   // _WIN32

int main(int argc, char** argv)
{
    gForceDesync = (argc > 1 && !strcmp(argv[1], "desync"));
    if (gForceDesync)
        printf("=== desync mode: the joiner will report a corrupted digest "
               "from step %u\n", gDesyncFrom);

    printf("net_lockstep_test needs fork(); skipped on Windows.\n");
    return 0;
}

#endif
