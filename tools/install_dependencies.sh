#!/usr/bin/env bash
set -euo pipefail

# Install build dependencies (Debian/Ubuntu best-effort).
# For other distros, we only print a clear message.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if command -v apt-get >/dev/null 2>&1; then
  echo "Detected Debian/Ubuntu (apt-get). Installing build dependencies..."
  echo "This may require sudo privileges."
  sudo apt-get update
  sudo apt-get install -y \
    build-essential gcc g++ make nasm pkg-config \
    zlib1g-dev liblz4-dev libzstd-dev \
    python3 python3-pip \
    dpkg-dev \
    libncurses5-dev dialog whiptail

  # Attempt static libs if available
  if apt-cache show libstdc++-static >/dev/null 2>&1; then
    sudo apt-get install -y libstdc++-static || true
  fi
  if apt-cache show glibc-source >/dev/null 2>&1; then
    echo "Note: glibc static may require manual build; skipping automatic install."
  fi

  echo "Done. If building static, ensure libc static is available on this system."
else
  echo "Non-Debian/Ubuntu system detected (no apt-get)."
  echo "Please install equivalent build deps manually:"
  echo "  - Toolchain: gcc, g++, make, nasm, pkg-config"
  echo "  - Libs: zlib (dev), lz4 (dev), zstd (dev)"
  echo "  - Python 3 + pip, dpkg-deb (for .deb), dialog/whiptail (for menu)"
  echo "  - For static builds: libstdc++ static / glibc static if available"
  exit 0
fi


