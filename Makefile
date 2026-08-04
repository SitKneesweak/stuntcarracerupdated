#
# General Compiler Settings
#

CC=g++
#PANDORA=1
#DEBUG=1

# general compiler settings
ifeq ($(M32),1)
	FLAGS= -m32
endif
ifeq ($(PANDORA),1)
	FLAGS= -mcpu=cortex-a8 -mfpu=neon -mfloat-abi=softfp -march=armv7-a -fsingle-precision-constant -mno-unaligned-access -fdiagnostics-color=auto -O3 -fsigned-char
	FLAGS+= -DPANDORA
	FLAGS+= -DARM
	LDFLAGS= -mcpu=cortex-a8 -mfpu=neon -mfloat-abi=softfp
	#HAVE_GLES=1
endif
ifeq ($(PYRA),1)
        FLAGS= -mcpu=cortex-a15 -mfpu=neon -mfloat-abi=hard -fsingle-precision-constant
        FLAGS+= -DPYRA
        FLAGS+= -DARM
        LDFLAGS= -mcpu=cortex-a8 -mfpu=neon -mfloat-abi=softfp
endif
ifeq ($(ODROID),1)
        FLAGS= -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard -fsingle-precision-constant -O3 -fsigned-char
        FLAGS+= -DODROID
        FLAGS+= -DARM
        LDFLAGS= -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard
        #HAVE_GLES=1
endif
ifeq ($(ODROIDN1),1)
        FLAGS= -mcpu=cortex-a72.cortex-a53 -fsingle-precision-constant -O3 -fsigned-char -ffast-math
        FLAGS+= -DODROID
        FLAGS+= -DARM
        LDFLAGS= -mcpu=cortex-a72.cortex-a53
        #HAVE_GLES=1
endif
ifeq ($(CHIP),1)
        FLAGS= -mcpu=cortex-a8 -mfpu=neon -mfloat-abi=hard -fsingle-precision-constant -O3 -fsigned-char
        FLAGS+= -DCHIP
        FLAGS+= -DARM
        LDFLAGS= -mcpu=cortex-a8 -mfpu=neon -mfloat-abi=hard
        #HAVE_GLES=1
endif
ifeq ($(LINUX),1)
        # Desktop Linux.
        FLAGS+= -DUSE_SDL2
        SDL=2
endif
ifeq ($(MINGW),1)
        # Windows, cross-compiled or under MSYS2.
        FLAGS+= -DUSE_SDL2
        SDL=2
endif
ifeq ($(MACOS),1)
        # macOS build. Uses Homebrew for SDL2, SDL2_ttf, openal-soft, glm.
        FLAGS+= -DUSE_SDL2 -DMACOS -DGL_SILENCE_DEPRECATION
        BREW_PREFIX ?= $(shell brew --prefix)
        export PKG_CONFIG_PATH := $(BREW_PREFIX)/opt/openal-soft/lib/pkgconfig:$(BREW_PREFIX)/opt/sdl2-compat/lib/pkgconfig:$(BREW_PREFIX)/opt/sdl2_ttf/lib/pkgconfig:$(PKG_CONFIG_PATH)
        FLAGS+= -I$(BREW_PREFIX)/include
        SDL=2
endif
ifeq ($(EMSCRIPTEN),1)
        FLAGS= -s FULL_ES2=1 -I../gl4es/include -s USE_SDL_TTF=2 -s USE_SDL=2
        FLAGS+= -I/usr/include/glm
        FLAGS+= -DUSE_SDL2
        FLAGS+= --emrun --preload-file Tracks --preload-file Sounds
        FLAGS+= --preload-file Bitmap --embed-file DejaVuSans-Bold.ttf
        FLAGS+= --shell-file template.html
        LDFLAGS= -s FULL_ES2=1 -s USE_SDL_TTF=2 -s USE_SDL=2
        CC= emcc
        CXX= emc++
endif

# Everything this Makefile builds is the SDL/OpenGL port. The DirectX build is the
# MSVC .vcxproj instead, and never sees this define - that is the distinction the
# source-level #ifdef SCR_PORTABLE draws. (It used to be spelt "#ifdef linux",
# which relied on a gcc builtin that MinGW does not set.)
FLAGS+= -DSCR_PORTABLE

# Bit-identical floating point across platforms, which lockstep multiplayer will
# depend on: the same inputs must produce the same doubles on clang/macOS,
# gcc/Linux and MinGW/Windows or peers desync.
#
# -ffp-contract=off is the one that actually bites. GCC defaults to "fast" for
# C++, letting it fuse a*b+c into a single FMA with different rounding - and
# whether it does so depends on target and optimizer decisions, so two platforms
# can disagree on one line of physics. Turning it off costs a little speed and
# buys reproducibility.
#
# Do NOT add -ffast-math (or -Ofast) to any build that runs the physics: it
# permits reassociation, which destroys reproducibility outright. Note the
# ARM64/cortex-a72 target above still sets it - that target predates this and is
# not one of the three cross-platform builds; it must lose the flag before it
# can join a netplay session.
FLAGS+= -ffp-contract=off

FLAGS+= -pipe -fpermissive
CFLAGS=$(FLAGS) -Wno-conversion-null -Wno-write-strings -ICommon
LDFLAGS=$(FLAGS)

ifeq ($(PANDORA),1)
	PROFILE=0
else
	PROFILE=0
endif


ifeq ($(DEBUG),1)
	FLAGS+= -g
	CFLAGS+=-Og
else
	CFLAGS+=-O3 -Winit-self
ifneq ($(MACOS),1)
	LDFLAGS+=-s
endif
endif

ifeq ($(PROFILE),1)
	ifneq ($(DEBUG),1)
		# Debug symbols needed for profiling to be useful
		FLAGS+= -g
	endif
	FLAGS+= -pg
endif

#SDL=1
ifeq ($(EMSCRIPTEN),1)
	GL4ES = ../gl4es/lib/libGL.a
	LIB+= -lopenal ${GL4ES}
else
ifeq ($(SDL),2)
	SDL_=sdl2
	TTF_ = SDL2_ttf
	CFLAGS += -DUSE_SDL2
else
	SDL_=
	CFLAGS+=`sdl-config --cflags`
	TTF_ = SDL_ttf
endif

# library headers
ifeq ($(PANDORA),1)
	CFLAGS+= `pkg-config --cflags $(SDL_) $(TTF_) openal`
else
	CFLAGS+= `pkg-config --cflags $(SDL_) $(TTF_) openal`
endif

# dynamic only libraries
ifeq ($(PANDORA),1)
	LIB+= `sdl-config --libs`
else
	LIB+= `pkg-config --libs $(SDL_)`
endif

LIB+= `pkg-config --libs $(TTF_)`

ifeq ($(MINGW),1)
	LIB += -lglu32 -lopengl32
	LIB += -lws2_32 -lwinmm
	LIB += `pkg-config --libs openal`
	# MSYS2's sdl2.pc puts -mwindows in Libs, which links a GUI-subsystem exe.
	# Those get no console, so every printf in the game is discarded - including
	# the GL/OpenAL diagnostics and the physics N/K dumps. This must come after
	# the pkg-config lines above: the last subsystem flag on the line wins.
	# Set WINCONSOLE=0 for a release build with no console window.
	ifneq ($(WINCONSOLE),0)
		LIB += -mconsole
	endif
else
ifeq ($(MACOS),1)
	LIB += -framework OpenGL
	LIB += `pkg-config --libs openal`
else
	ifeq ($(HAVE_GLES),1)
		LIB += -lGLES_CM -lEGL
		CFLAGS += -DHAVE_GLES
	else
		LIB += -lGL -lGLU
	endif
	LIB += -lopenal
endif
endif
ifneq ($(MINGW),1)
ifneq ($(MACOS),1)
	# apparently on some systems -ldl is explicitly required
	# perhaps this is part of the default libs on others...?
	LIB+= -ldl
endif
endif
endif

# specific includes
CFLAGS += -I.
CFLAGS += -DSOUND_OPENAL

ifeq ($(DEBUG),1)
	CFLAGS+= -DDEBUG_ON -DDEBUG_COMP -DDEBUG_SPOTFX_SOUND -DDEBUG_VIEWPORT
endif

ifeq ($(EMSCRIPTEN),1)
BIN=docs/index.html
else
BIN=stuntcarracer
endif


INC=$(wildcard *.h)
SRC=$(wildcard *.cpp)
ifeq ($(EMSCRIPTEN),1)
OBJ=$(patsubst %.cpp,%.bc,$(SRC))
#Not in OBJ to avoid removal with a "clean" command
INC+=${GL4ES}
else
OBJ=$(patsubst %.cpp,%.o,$(SRC))
endif

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) -o $(BIN) $(OBJ) $(CFLAGS) $(LDFLAGS) $(LIB)

$(OBJ): $(INC)

%.o: %.cpp
	$(CC) -o $@ -c $< $(CFLAGS)

%.bc: %.cpp
	$(CC) -o $@ -c $< $(CFLAGS)

clean:
	$(RM) $(OBJ) $(BIN)

check:
	@echo
	@echo "INC = $(INC)"
	@echo
	@echo "SRC = $(SRC)"
	@echo
	@echo "OBJ = $(OBJ)"
	@echo
	@echo "DEBUG = $(DEBUG)"
	@echo "PROFILE = $(PROFILE)"
	@echo "PANDORA = $(PANDORA)"
	@echo "ODROID = $(ODROID)"
	@echo "CHIP = $(CHIP)"
	@echo "HAVE_GLES = $(HAVE_GLES)"
	@echo "SDL = $(SDL)"
	@echo "SDL_ = $(SDL_)"
	@echo
	@echo "CC = $(CC)"
	@echo "BIN = $(BIN)"
	@echo "CFLAGS = $(CFLAGS)"
	@echo "LDFLAGS = $(LDFLAGS)"
	@echo "LIB = $(LIB)"
	@echo
