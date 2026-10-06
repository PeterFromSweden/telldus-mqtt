#!/bin/bash
# Installs everything needed to build and test telldus-mqtt on Debian/Ubuntu:
# mosquitto, cJSON and telldus-core (built from PeterFromSweden/telldus).
# Idempotent and non-interactive. Run as root or with passwordless sudo.
set -euo pipefail

SUDO=""
if [ "$(id -u)" -ne 0 ]; then
  SUDO="sudo -n"
fi

PACKAGES="build-essential cmake pkg-config mosquitto mosquitto-clients libmosquitto-dev libcjson-dev libconfuse-dev libftdi-dev"
MISSING=""
for p in $PACKAGES; do
  dpkg -s "$p" >/dev/null 2>&1 || MISSING="$MISSING $p"
done
if [ -n "$MISSING" ]; then
  echo "Installing:$MISSING"
  $SUDO apt-get update -qq
  DEBIAN_FRONTEND=noninteractive $SUDO apt-get install -y -qq $MISSING
fi

if [ ! -f /usr/include/telldus-core.h ] || ! ls /usr/lib/libtelldus-core.so* >/dev/null 2>&1; then
  TELLDUS_SRC="${TELLDUS_SRC:-${XDG_CACHE_HOME:-$HOME/.cache}/telldus-src}"
  if [ ! -d "$TELLDUS_SRC/.git" ]; then
    git clone --depth 1 https://github.com/PeterFromSweden/telldus.git "$TELLDUS_SRC"
  fi
  # telldus-core's CMake has a target ordering race, so build with one job.
  cmake -S "$TELLDUS_SRC/telldus-core" -B "$TELLDUS_SRC/telldus-core/build" -DCMAKE_INSTALL_PREFIX=/usr
  cmake --build "$TELLDUS_SRC/telldus-core/build" -j1
  $SUDO cmake --install "$TELLDUS_SRC/telldus-core/build"
  $SUDO ldconfig
fi

echo "Dependencies ready. Build with: cmake -B build && cmake --build build && (cd build && ctest)"
