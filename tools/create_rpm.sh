#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
STATIC_BUILD="${STATIC:-0}"

if ! command -v nfpm >/dev/null 2>&1; then
  echo "Error: nfpm not found. Install nfpm to create .rpm packages." >&2
  exit 1
fi

echo "Building .rpm package for zELF packer"

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
if ! (cd "$ROOT_DIR" && "$BUILD_DIR/zelf" -lzma "$BUILD_DIR/zelf" --output "$RELEASE_BIN" >/dev/null 2>&1); then
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

HOST_UNAME_M=$(uname -m 2>/dev/null || echo unknown)
NFPM_ARCH=amd64
FILE_ARCH=amd64
case "$HOST_UNAME_M" in
  x86_64)
    NFPM_ARCH=amd64
    FILE_ARCH=amd64
    ;;
  aarch64)
    NFPM_ARCH=arm64
    FILE_ARCH=aarch64
    ;;
  arm64)
    NFPM_ARCH=arm64
    FILE_ARCH=arm64
    ;;
  *)
    NFPM_ARCH="$HOST_UNAME_M"
    FILE_ARCH="$HOST_UNAME_M"
    ;;
esac

PKGNAME=zelf
PKG_STATIC_SUFFIX=""
if [ "$STATIC_BUILD" = "1" ]; then
  PKG_STATIC_SUFFIX="_static"
fi

OUT_RPM="$BUILD_DIR/${PKGNAME}${PKG_STATIC_SUFFIX}_${ZELF_VERSION}_${TIMESTAMP}_${FILE_ARCH}.rpm"

NFPM_CFG="$(mktemp)"
trap 'rm -f "$NFPM_CFG"' EXIT

cat > "$NFPM_CFG" <<EOF
name: "zELF"
arch: "$NFPM_ARCH"
platform: "linux"
version: "${ZELF_VERSION_RAW:-${ZELF_VERSION}}"
section: "default"
priority: "extra"
maintainer: "seb3773 <seb3773@gmail.com>"
description: "Zelf binary packer"
license: "GPL3"
contents:
  - src: "$BUILD_DIR/zelf"
    dst: "/usr/bin/zelf"
  - src: "$ROOT_DIR/README.md"
    dst: "/usr/share/doc/zelf/README.md"
EOF

echo "Building $OUT_RPM"
if ! nfpm pkg -f "$NFPM_CFG" -p rpm -t "$OUT_RPM"; then
  echo "Error: nfpm failed to create rpm" >&2
  exit 1
fi

RPM_BASENAME="$(basename "$OUT_RPM")"
RPM_DIRNAME="$(dirname "$OUT_RPM")"
printf "\033[1;32m%s\033[0m generated in %s\n" "$RPM_BASENAME" "$RPM_DIRNAME"
