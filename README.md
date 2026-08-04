# stuntcarremake

[![stuntcarremake Linux build status](https://travis-ci.org/ptitSeb/stuntcarremake.svg?branch=master)](https://travis-ci.org/ptitSeb/stuntcarremake "stuntcarremake Linux build status") [![stuntcarremake Windows build status](https://ci.appveyor.com/api/projects/status/3b9bd69a4vsy0eu6/branch/master?svg=true)](https://ci.appveyor.com/project/ptitSeb/stuntcarremake/branch/master "stuntcarremake Windows Build status")

This is a port to Linux & OpenPandora of Stunt Car Racer Remake, a windows remake of the old Stunt Car Racer from the AtariST/Amiga time.

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

You can play Emscripten version, built using [gl4es](https://github.com/ptitSeb/gl4es) here: [Web version](http://ptitseb.github.io/stuntcarremake/)

Some code (the OpenAL part) come from Forsaken/ProjectX port by chino.

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

Original project is here: http://sourceforge.net/projects/stuntcarremake/
