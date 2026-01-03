#!/usr/bin/env bash
set -euo pipefail

# Create a minimal Debian package containing the packer binary `build/zelf`
# Installs binary to /usr/bin/zelf

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
PKG_DIR="$BUILD_DIR/package"
# Detect static intent from make (STATIC=1) – default 0
STATIC_BUILD="${STATIC:-0}"

echo "Building .deb package for zELF packer"

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


if ! command -v dpkg-deb >/dev/null 2>&1; then
  echo "Error: dpkg-deb not found. Install 'dpkg' package to create .deb files." >&2
  exit 1
fi

# extract zELF version from header, fall back to timestamp-only if missing
HEADER="$ROOT_DIR/src/packer/zelf_packer.h"
ZELF_VERSION_RAW=""
if [ -f "$HEADER" ]; then
  # expects a line like: #define ZELF_VERSION "v0.8"
  ZELF_VERSION_RAW=$(grep -E '^#define[[:space:]]+ZELF_VERSION' "$HEADER" || true)
  ZELF_VERSION_RAW=$(echo "$ZELF_VERSION_RAW" | sed -E 's/.*"([^"]+)".*/\1/')
fi

# If we failed to extract a version, use timestamp as a fallback
TIMESTAMP=$(date +%Y%m%d%H%M)
if [ -z "$ZELF_VERSION_RAW" ]; then
  ZELF_VERSION="v${TIMESTAMP}"
else
  # sanitize version for use in filenames: replace dots with underscores
  ZELF_VERSION=$(echo "$ZELF_VERSION_RAW" | tr '.' '_')
fi

ARCH=amd64
PKGNAME=zelf
PKG_STATIC_SUFFIX=""
if [ "$STATIC_BUILD" = "1" ]; then
  PKG_STATIC_SUFFIX="_static"
fi
OUT_DEB="$BUILD_DIR/${PKGNAME}${PKG_STATIC_SUFFIX}_${ZELF_VERSION}_${TIMESTAMP}_${ARCH}.deb"

rm -rf "$PKG_DIR"
mkdir -p "$PKG_DIR/DEBIAN"
mkdir -p "$PKG_DIR/usr/bin"

cat > "$PKG_DIR/DEBIAN/control" <<EOF
Package: $PKGNAME
Version: ${ZELF_VERSION_RAW:-$ZELF_VERSION}-${TIMESTAMP}
Section: utils
Priority: optional
Architecture: $ARCH
Maintainer: zELF packer <noreply@example.com>
Depends: 
Description: zELF ELF packer (packer only)
 A minimal package containing the zELF packer binary.
EOF

cp "$BUILD_DIR/zelf" "$PKG_DIR/usr/bin/zelf"
chmod 0755 "$PKG_DIR/usr/bin/zelf"

echo "Building $OUT_DEB"
dpkg-deb --build "$PKG_DIR" "$OUT_DEB"

DEB_BASENAME="$(basename "$OUT_DEB")"
DEB_DIRNAME="$(dirname "$OUT_DEB")"
printf "\033[1;32m%s\033[0m generated in %s\n" "$DEB_BASENAME" "$DEB_DIRNAME"

# remove temporary package directory after building the .deb
rm -rf "$PKG_DIR"
