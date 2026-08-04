# Stunt Car Racer Updated

**Work in progress.** This is a personal fan project: an in-progress fork that tries to
bring the remake closer to the original Amiga game, and to add a few things the original
never had.

## Attribution — this is not my work

Stunt Car Racer was created by **Geoff Crammond** (MicroProse / MicroStyle, 1989). All
rights to the game, its design, and its name belong to the original author and rights
holders. I claim no ownership of any of it.

This repository is a fork of work by other people, and the overwhelming majority of the
code here is theirs:

- **Stunt Car Racer Remake** — the original Windows/DirectX remake this all descends from:
  http://sourceforge.net/projects/stuntcarremake/
- **[ptitSeb/stuntcarremake](https://github.com/ptitSeb/stuntcarremake)** — the Linux /
  OpenPandora / Emscripten port that this repo is forked from (`upstream`).
- The OpenAL sound code comes from the Forsaken / ProjectX port by **chino**.
- Windows resizing / UI-scaling work by **omenoid** <akaunist@gmail.com>.
- The FloatV2 physics is a port of the reworked physics published at stuntcarracer.net.

My changes are the ones listed under "What's new here" below. Everything else is the
original authors' work, kept under whatever terms they released it. This fork is
non-commercial and exists for the love of the game. If any rights holder would prefer it
not be public, say the word and it comes down.

## What's new here (work in progress)

Nothing below should be considered finished — this is an active branch and things break.

- **Internet / LAN multiplayer.** Head-to-head racing over UDP with a deterministic
  lockstep protocol: two players race the same track and see each other's car. Includes a
  host/join menu, the host's own address shown on the wait screen, and both cars craned in
  from opposite sides at the start. Still rough around the edges.
- **Deterministic physics.** Required for lockstep: reproducible `sin`/`cos`/`pow`
  (`Det_Math.h`), `-ffp-contract=off`, a per-step physics checksum, and a `--simtrace`
  mode with a pasteable digest for hunting divergence between two machines.
- **FloatV2 physics port.** A port of the floating-point physics rework in place of the
  original integer sim, with a runtime toggle (F11).
- **New car visuals**, updated menu artwork, driver portraits, and menu screen alignment.
- **Closer to the Amiga.** Amiga field of view and camera pitch, PAL pixel aspect with a
  selectable display aspect, sharp-bilinear 2D filtering, volumetric fog, drawbridge and
  scenery/horizon seam fixes, and the crane drop-in start with chains and hoist.
- **Season / league changes.** League or Super League when starting a season, revised
  league setup, and damage that persists if you fall off the track.
- **Cross-platform.** Native macOS support, a runtime GL loader, a working MinGW Windows
  SDL build, and CI that uploads a runnable build per platform.

## Building

Desktop targets, all SDL2 + OpenGL + OpenAL:

| Platform | Command | Dependencies |
| --- | --- | --- |
| Linux | `make LINUX=1` | `libsdl2-dev libsdl2-ttf-dev libopenal-dev libglm-dev libgl1-mesa-dev libglu1-mesa-dev` |
| macOS | `make MACOS=1` | `brew install sdl2 sdl2_ttf openal-soft glm pkg-config` |
| Windows | `make MINGW=1` | MSYS2 MINGW64: `mingw-w64-x86_64-{gcc,SDL2,SDL2_ttf,openal,glm}` |

Handheld targets, which still default to SDL1: `make ODROID=1`, `make CHIP=1`, or a
plain `make` for Pandora. Add `SDL=2` to any of those to use SDL2 instead.

All of the above build the same portable renderer, selected by `-DSCR_PORTABLE`
(see the note in the Makefile). The separate DirectX 9 build is the MSVC
`StuntCarRacer_2022.vcxproj`, which is currently stale - it predates most of the
source files and does not link.

An Emscripten build of the *upstream* project, using
[gl4es](https://github.com/ptitSeb/gl4es), is playable here:
[Web version](http://ptitseb.github.io/stuntcarremake/)

## Windows-Specific Features

The Windows build includes additional features not available on Linux:

### Dynamic Window Resizing
- **Resizable window** with automatic aspect ratio preservation (16:10 for widescreen, 4:3 for standard)
- **Dynamic font scaling** - fonts and UI text scale based on window size
- **Dynamic cockpit scaling** - cockpit overlay scales proportionally with window resolution
- **Intelligent positioning** - UI elements maintain proper positioning at any resolution

**Technical Details:**
- Implemented via Win32 `WM_SIZING` message handler
- Uses DirectX backbuffer dimensions for accurate scaling calculations
- Base resolutions: 800x480 (widescreen) or 640x480 (standard)
- All scaling factors calculated dynamically from current vs base resolution

**Linux Status:** Linux builds use fixed resolution (800x480 or 640x480) as the SDL implementation does not currently support dynamic resizing. The scaling code is present but inactive (returns 1.0x scale) to maintain compatibility.

**Contributors:** Windows enhancements by omenoid <akaunist@gmail.com> (2025-11-30)

## Controls

Controls are:
On Linux / Windows
 4 Arrows for Turning / Accelerate / Brake
 Space for Boost

On Pandora
 DPad Left/Right for turning
 (X) Accelerate
 (B) Brake
 (R) Boost

[![Play on Youtube](https://img.youtube.com/vi/qKTFntQtG6E/0.jpg)](https://www.youtube.com/watch?v=qKTFntQtG6E)

Here is a video on StuntCarRemake running on the OpenPandora
