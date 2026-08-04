@echo off
REM ---------------------------------------------------------------------------
REM Runs the determinism trace on all eight tracks and collects the digests.
REM
REM This exists because --simtrace is a command-line flag, and the Windows build
REM is used on a machine where opening a prompt in the right folder is more
REM friction than it is worth. Double-click this instead.
REM
REM Each track drives itself into a race, runs 6000 physics steps (100 seconds
REM of simulated time, about 15 seconds of real time) and quits. Nothing to
REM play, and the window will open and close eight times.
REM
REM What matters is simtrace-digests.txt: eight lines, one per track. If they
REM match the other machine's, the physics is bit-identical everywhere. Each
REM track also leaves a simtrace-track<N>.log holding a checkpoint every 100
REM steps, which is what narrows a mismatch down to a 100-step window.
REM ---------------------------------------------------------------------------

cd /d "%~dp0"

set OUT=simtrace-digests.txt
echo simtrace digests > "%OUT%"

for %%T in (0 1 2 3 4 5 6 7) do (
    echo Tracing track %%T of 7, please wait...
    stuntcarracer.exe --simtrace-digest --simtrace-track %%T --simtrace-out simtrace-track%%T.log > nul 2>&1
    for /f "tokens=3,4" %%A in ('findstr /b "# DIGEST" simtrace-track%%T.log') do (
        echo track %%T  %%B >> "%OUT%"
    )
)

echo.
echo ---------------------------------------------------------------------------
type "%OUT%"
echo ---------------------------------------------------------------------------
echo.
echo Written to "%~dp0%OUT%" - send that file.
pause
