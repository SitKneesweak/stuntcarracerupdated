@echo off
REM ---------------------------------------------------------------------------
REM Runs the determinism trace and leaves the result on screen.
REM
REM This exists because --simtrace is a command-line flag, and the Windows build
REM is used on a machine where opening a prompt in the right folder is more
REM friction than it is worth. Double-click this instead.
REM
REM It drives itself into a race, runs 6000 physics steps (100 seconds of
REM simulated time, about 16 seconds of real time) and quits. Nothing to play.
REM
REM What matters is the single "DIGEST" line printed at the end: if it matches
REM the other machine's, every bit of every step agreed. simtrace.log beside the
REM exe holds the same line plus a checkpoint every 100 steps, for narrowing
REM down a mismatch.
REM ---------------------------------------------------------------------------

cd /d "%~dp0"

echo Running determinism trace, please wait (about 15 seconds)...
echo.

stuntcarracer.exe --simtrace-digest %*

echo.
echo ---------------------------------------------------------------------------
echo Full log: "%~dp0simtrace.log"  (65 lines)
echo Copy the DIGEST line above, or the whole log file.
echo ---------------------------------------------------------------------------
pause
