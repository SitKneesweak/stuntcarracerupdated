#!/bin/bash
#
# Cross-build the Windows x64 executable on macOS, and package it ready to copy
# to the PC. Run from anywhere: tools/build-win.sh [extra make args]
#
# This exists because head-to-head testing needs a Windows build from the *same*
# source as the Mac one - kNetSimVersion and the simulation have to match on both
# ends - and waiting on GitHub Actions for that is a slow way to find out.
#
# Needs: brew install mingw-w64 glm
# Everything else (SDL2, SDL2_ttf, OpenAL Soft, all built for Windows) is fetched
# once into $SYSROOT below and reused. Delete that directory to start again.

set -e
cd "$(dirname "$0")/.."
SRC=$(pwd)

SYSROOT=${SCR_MINGW_SYSROOT:-$HOME/.scr-mingw/sysroot}
SDL_VER=2.32.10
TTF_VER=2.24.0
AL_VER=1.24.3

if [ ! -f "$SYSROOT/lib/pkgconfig/sdl2.pc" ]; then
	echo "Building the MinGW sysroot in $SYSROOT (one time only)..."
	DL=$(dirname "$SYSROOT")/dl
	mkdir -p "$DL" "$SYSROOT/lib/pkgconfig" "$SYSROOT/include/AL" "$SYSROOT/bin"
	cd "$DL"
	curl -sSL -O "https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VER/SDL2-devel-$SDL_VER-mingw.tar.gz"
	curl -sSL -O "https://github.com/libsdl-org/SDL_ttf/releases/download/release-$TTF_VER/SDL2_ttf-devel-$TTF_VER-mingw.tar.gz"
	curl -sSL -O "https://github.com/kcat/openal-soft/releases/download/$AL_VER/openal-soft-$AL_VER-bin.zip"
	tar xzf "SDL2-devel-$SDL_VER-mingw.tar.gz"
	tar xzf "SDL2_ttf-devel-$TTF_VER-mingw.tar.gz"
	unzip -q -o "openal-soft-$AL_VER-bin.zip"
	cp -R "SDL2-$SDL_VER/x86_64-w64-mingw32/"* "$SYSROOT/"
	cp -R "SDL2_ttf-$TTF_VER/x86_64-w64-mingw32/"* "$SYSROOT/"

	# The upstream .pc files carry the prefix of whatever machine built them.
	for f in "$SYSROOT/lib/pkgconfig/sdl2.pc" "$SYSROOT/lib/pkgconfig/SDL2_ttf.pc"; do
		perl -pi -e "s{^prefix=.*}{prefix=$SYSROOT};" "$f"
		perl -pi -e "s{/tmp/tardir/[^ ]*/install-x86_64-w64-mingw32}{$SYSROOT}g" "$f"
	done

	# OpenAL Soft ships no pkg-config file, and its DLL is named for the loader
	# it is meant to replace rather than for itself.
	cp "openal-soft-$AL_VER-bin/include/AL/"*.h "$SYSROOT/include/AL/"
	cp "openal-soft-$AL_VER-bin/libs/Win64/libOpenAL32.dll.a" "$SYSROOT/lib/"
	cp "openal-soft-$AL_VER-bin/bin/Win64/soft_oal.dll" "$SYSROOT/bin/OpenAL32.dll"
	cat > "$SYSROOT/lib/pkgconfig/openal.pc" <<EOF
prefix=$SYSROOT
libdir=\${prefix}/lib
includedir=\${prefix}/include
Name: OpenAL
Description: OpenAL Soft, MinGW cross build
Version: $AL_VER
Cflags: -I\${includedir}
Libs: -L\${libdir} -lOpenAL32
EOF

	# glm is header-only, so Homebrew's copy is the Windows copy too. -L
	# because /opt/homebrew/include/glm is a symlink into the Cellar.
	cp -RL "$(brew --prefix)/include/glm/" "$SYSROOT/include/glm/"
	cd "$SRC"
fi

# Build in a copy, so a Windows .o never lands next to a Mac one - they share
# names and the Makefile cannot tell them apart.
OUT=${SCR_WIN_OUT:-$SRC/build-win}
rm -rf "$OUT"
mkdir -p "$OUT"
cp -R "$SRC/." "$OUT/src"
rm -f "$OUT/src"/*.o "$OUT/src/stuntcarracer" "$OUT/src/stuntcarracer.exe"
rm -rf "$OUT/src/build-win"

export PKG_CONFIG_PATH=$SYSROOT/lib/pkgconfig
export PKG_CONFIG_LIBDIR=$SYSROOT/lib/pkgconfig
export CPATH=$SYSROOT/include		# glm and AL, which no .pc file points at

# -static-lib*: without them the exe wants libstdc++-6.dll and libgcc_s_seh-1.dll
# from the Homebrew toolchain, which no Windows machine has.
make -C "$OUT/src" MINGW=1 \
	CC="x86_64-w64-mingw32-g++ -static-libgcc -static-libstdc++" \
	-j"$(sysctl -n hw.ncpu)" "$@"

DIST=$OUT/StuntCarRacer-win64
mkdir -p "$DIST"
cp "$OUT/src/stuntcarracer.exe" "$DIST/StuntCarRacer.exe"
cp -R "$SRC/Tracks" "$SRC/Sounds" "$SRC/Bitmap" "$SRC/DejaVuSans-Bold.ttf" "$DIST/"
cp "$SYSROOT/bin/SDL2.dll" "$SYSROOT/bin/SDL2_ttf.dll" "$SYSROOT/bin/OpenAL32.dll" "$DIST/"
# The one runtime DLL -static-libgcc cannot fold in.
cp "$(dirname "$(command -v x86_64-w64-mingw32-g++)")/../Cellar"/mingw-w64/*/toolchain-x86_64/x86_64-w64-mingw32/bin/libwinpthread-1.dll "$DIST/" 2>/dev/null \
	|| find "$(brew --prefix)/Cellar/mingw-w64" -name libwinpthread-1.dll -path "*toolchain-x86_64*" -exec cp {} "$DIST/" \;

cd "$OUT" && rm -f StuntCarRacer-win64.zip && zip -qr StuntCarRacer-win64.zip StuntCarRacer-win64
echo
echo "Windows build ready: $OUT/StuntCarRacer-win64.zip"
