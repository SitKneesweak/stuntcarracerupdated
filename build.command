#!/bin/bash
# Build Stunt Car Racer.
# Double-click in Finder, or run: ./build.command [extra make args]
cd "$(dirname "$0")"

# Keep the Terminal window open long enough to read the result.
trap 'echo; printf "Press return to close..."; read -r _' EXIT

JOBS=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo "Building with make -j$JOBS $*"
echo
if make -j"$JOBS" "$@"; then
	echo
	echo "Build succeeded."
	printf "Run the game now? [y/N] "
	read -r ANS
	case "$ANS" in
		y|Y) ./stuntcarracer ;;
	esac
else
	echo
	echo "BUILD FAILED (see errors above)."
	exit 1
fi
