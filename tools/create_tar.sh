#!/usr/bin/env bash
set -euo pipefail

# Create a tar.gz package containing the packed packer binary and README.md
# Archive name:
#   zelf_<version>_<timestamp>_amd64.tar.gz
#   zelf_static_<version>_<timestamp>_amd64.tar.gz if STATIC=1

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
STATIC_BUILD="${STATIC:-0}"

echo "Building tar.gz package for zELF packer"

if [ ! -x "$BUILD_DIR/zelf" ]; then
  echo "Packer binary not found or not executable: $BUILD_DIR/zelf"
  echo "Attempting to build packer..."
  (cd "$ROOT_DIR" && make packer)
fi

if [ ! -x "$BUILD_DIR/zelf" ]; then
  echo "Error: build/zelf still missing after make packer" >&2
  exit 1
fi

# Create a release-friendly self-packed binary before packaging
RELEASE_BIN="$BUILD_DIR/zelf_release"
echo "Creating self-packed release binary: $RELEASE_BIN"
if ! (cd "$ROOT_DIR" && "$BUILD_DIR/zelf" -shr "$BUILD_DIR/zelf" --output "$RELEASE_BIN" >/dev/null 2>&1); then
  echo "Error: failed to create release binary with packer" >&2
  exit 1
fi

if [ ! -f "$RELEASE_BIN" ]; then
  echo "Error: expected release binary not found: $RELEASE_BIN" >&2
  exit 1
fi

echo "Replacing $BUILD_DIR/zelf with release binary"
mv -f "$RELEASE_BIN" "$BUILD_DIR/zelf"
chmod 0755 "$BUILD_DIR/zelf"

# extract zELF version from header, fall back to timestamp-only if missing
HEADER="$ROOT_DIR/src/packer/zelf_packer.h"
ZELF_VERSION_RAW=""
if [ -f "$HEADER" ]; then
  ZELF_VERSION_RAW=$(grep -E '^#define[[:space:]]+ZELF_VERSION' "$HEADER" || true)
  ZELF_VERSION_RAW=$(echo "$ZELF_VERSION_RAW" | sed -E 's/.*"([^"]+)".*/\1/')
fi

TIMESTAMP=$(date +%Y%m%d%H%M)
if [ -z "$ZELF_VERSION_RAW" ]; then
  ZELF_VERSION="v${TIMESTAMP}"
else
  ZELF_VERSION=$(echo "$ZELF_VERSION_RAW" | tr '.' '_')
fi

ARCH=amd64
PKGNAME=zelf
PKG_STATIC_SUFFIX=""
if [ "$STATIC_BUILD" = "1" ]; then
  PKG_STATIC_SUFFIX="_static"
fi
OUT_TAR="$BUILD_DIR/${PKGNAME}${PKG_STATIC_SUFFIX}_${ZELF_VERSION}_${TIMESTAMP}_${ARCH}.tar.gz"

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

cp "$BUILD_DIR/zelf" "$TMP_DIR/zelf"
chmod 0755 "$TMP_DIR/zelf"
if [ -f "$ROOT_DIR/README.md" ]; then
  cp "$ROOT_DIR/README.md" "$TMP_DIR/README.md"
fi

echo "Building $OUT_TAR"
tar -C "$TMP_DIR" -czf "$OUT_TAR" .

TAR_BASENAME="$(basename "$OUT_TAR")"
TAR_DIRNAME="$(dirname "$OUT_TAR")"
printf "\033[1;32m%s\033[0m generated in %s\n" "$TAR_BASENAME" "$TAR_DIRNAME"

rm -rf "$TMP_DIR"

