#!/bin/sh
set -eu

CC_BIN="${CC:-gcc}"

OUTDIR="tools/binaries_sample_arm64"

mkdir -p "$OUTDIR"

CFLAGS_BASE="-O2 -march=armv8-a -fomit-frame-pointer -fstrict-aliasing -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-stack-protector -fno-plt -fno-ident -Wall -Wextra -Wshadow"

"$CC_BIN" $CFLAGS_BASE -static -s tools/simple_static.c -o "$OUTDIR/simple_static"
"$CC_BIN" $CFLAGS_BASE -fno-pie -no-pie -s tools/dynexec_probe.c -o "$OUTDIR/dynexec_probe"
