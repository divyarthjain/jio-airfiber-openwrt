#!/bin/sh
# Builds browser Doom: build/doom-web for the router (aarch64 musl, static) and
# build/doom-web-host for this machine (used by check.py). Also fetches Freedoom.
# Needs git, curl, a C compiler, and uv (it fetches the Zig compiler to cross-compile).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
B="$HERE/build"
DOOM_ASCII_COMMIT=940528f138e12431e5a1417351863709f1620078
FREEDOOM=0.13.0
FREEDOOM_SHA256=3f9b264f3e3ce503b4fb7f6bdcb1f419d93c7b546f4df3e874dd878db9688f59
FLAGS="-O2 -w -std=c99 -DNORMALUNIX -DLINUX -D_DEFAULT_SOURCE -DVERSION=0.3.2"

mkdir -p "$B"
[ -d "$B/doom-ascii" ] || git clone -q https://github.com/wojciech-graj/doom-ascii.git "$B/doom-ascii"
git -C "$B/doom-ascii" checkout -q "$DOOM_ASCII_COMMIT"
cp "$HERE/doomgeneric_web.c" "$B/doom-ascii/src/"
cp "$B/doom-ascii/src/.default.cfg" "$B/"

cd "$B/doom-ascii/src"
SRC=$(sed -n '/^SRC = /,/doomgeneric_ascii.c/p' ../Makefile | sed 's/^SRC = //; s/\\//g; s/doomgeneric_ascii\.c/doomgeneric_web.c/')
echo "building host binary"
cc $FLAGS $SRC -o "$B/doom-web-host" -lm
echo "building router binary (aarch64-linux-musl)"
uvx --from ziglang python -m ziglang cc -target aarch64-linux-musl $FLAGS -static -s $SRC -o "$B/doom-web" -lm

cd "$B"
if [ ! -s freedoom1.wad ]; then
	echo "fetching Freedoom $FREEDOOM"
	curl -sL -o freedoom.zip "https://github.com/freedoom/freedoom/releases/download/v$FREEDOOM/freedoom-$FREEDOOM.zip"
	echo "$FREEDOOM_SHA256  freedoom.zip" | shasum -a 256 -c -
	unzip -o -q -j freedoom.zip "freedoom-$FREEDOOM/freedoom1.wad"
	rm freedoom.zip
fi
ls -la "$B/doom-web" "$B/doom-web-host" "$B/freedoom1.wad"
