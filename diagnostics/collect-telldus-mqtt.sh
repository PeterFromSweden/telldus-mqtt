#!/bin/sh
# collect-telldus-mqtt.sh
# BusyBox/OpenWrt-friendly diagnostic collector for telldus-mqtt
# Produces a tarball in /tmp and prints its path and sha256 checksum.

set -eu

TS=$(date +%s)
OUTDIR="/tmp/telldus-mqtt-diag-${TS}"
mkdir -p "$OUTDIR"

echo "collecting into $OUTDIR"

# Logs and service
if command -v logread >/dev/null 2>&1; then
  logread > "$OUTDIR/logread.full" 2>/dev/null || true
  grep telldus-mqtt "$OUTDIR/logread.full" > "$OUTDIR/logread.telldus-mqtt" 2>/dev/null || true
else
  # Fallback: /var/log messages
  [ -f /var/log/messages ] && cp -a /var/log/messages "$OUTDIR/logread.full" 2>/dev/null || true
fi

# Configs and service script
[ -d /etc/telldus-mqtt ] && cp -a /etc/telldus-mqtt "$OUTDIR/" 2>/dev/null || true
[ -f /etc/init.d/telldus-mqtt ] && cp -a /etc/init.d/telldus-mqtt "$OUTDIR/" 2>/dev/null || true

# Runtime & network
ps > "$OUTDIR/ps.txt" 2>/dev/null || true
if command -v ip >/dev/null 2>&1; then
  ip addr > "$OUTDIR/ip_addr.txt" 2>/dev/null || true
else
  if command -v ifconfig >/dev/null 2>&1; then
    ifconfig > "$OUTDIR/ifconfig.txt" 2>/dev/null || true
  fi
fi

if command -v ss >/dev/null 2>&1; then
  ss -ltnp > "$OUTDIR/net.txt" 2>/dev/null || true
else
  if command -v netstat >/dev/null 2>&1; then
    netstat -tulpen > "$OUTDIR/net.txt" 2>/dev/null || true
  fi
fi

# System & package info
uname -a > "$OUTDIR/uname.txt" 2>/dev/null || true
[ -f /etc/os-release ] && cp -a /etc/os-release "$OUTDIR/" 2>/dev/null || true
if command -v opkg >/dev/null 2>&1; then
  opkg list-installed > "$OUTDIR/opkg-list.txt" 2>/dev/null || true
fi
df -h > "$OUTDIR/df.txt" 2>/dev/null || true
if command -v free >/dev/null 2>&1; then
  free -m > "$OUTDIR/free.txt" 2>/dev/null || true
fi
dmesg | tail -n 200 > "$OUTDIR/dmesg.tail" 2>/dev/null || true

# Binary and dependency info (if present)
if command -v telldus-mqtt >/dev/null 2>&1; then
  command -v telldus-mqtt > "$OUTDIR/telldus-mqtt.path" 2>/dev/null || true
  readelf -d "$(command -v telldus-mqtt)" 2>/dev/null | grep NEEDED > "$OUTDIR/telldus-mqtt.needed" 2>/dev/null || true
fi

# Package into tar.gz
TARBALL="/tmp/telldus-mqtt-diag-${TS}.tar.gz"
cd /tmp
tar czf "$TARBALL" "$(basename "$OUTDIR")" 2>/dev/null || tar -cf "$TARBALL" "$(basename "$OUTDIR")"

if command -v sha256sum >/dev/null 2>&1; then
  sha256sum "$TARBALL" > "${TARBALL}.sha256" 2>/dev/null || true
  echo "$TARBALL"
  echo "sha256: $(cat ${TARBALL}.sha256 2>/dev/null || true)"
else
  echo "$TARBALL"
  echo "sha256: (sha256sum not available on device)"
fi

echo "done"
