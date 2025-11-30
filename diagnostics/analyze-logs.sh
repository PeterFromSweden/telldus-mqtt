#!/bin/sh
# analyze-logs.sh
# Extracts diagnostic tarballs and analyzes telldus-mqtt exit/restart reasons
#
# Categories:
# 1) telldusd connection lost
# 2) mosquitto connection lost  
# 3) watchdog restart
# 4) other/unknown
#
# Usage:
#   ./analyze-logs.sh <collected-dir>

set -eu

if [ "$#" -lt 1 ]; then
  echo "Usage: $0 <collected-dir>" >&2
  exit 1
fi

COLLECTED_DIR="$1"

if [ ! -d "$COLLECTED_DIR" ]; then
  echo "Directory not found: $COLLECTED_DIR" >&2
  exit 2
fi

echo "Analyzing diagnostics in: $COLLECTED_DIR"
echo

# Extract all tarballs
cd "$COLLECTED_DIR"
for tarball in *.tar.gz; do
  [ -f "$tarball" ] || continue
  echo "Extracting: $tarball"
  tar xzf "$tarball" 2>/dev/null || true
done

echo
echo "=== RESTART REASON ANALYSIS ==="
echo

# Analyze each extracted directory
for diagdir in telldus-mqtt-diag-*; do
  [ -d "$diagdir" ] || continue
  
  # Extract device IP from tarball name (if available)
  DEVICE=$(echo "$diagdir" | sed 's/.*-\([0-9.]\+\)$/\1/' || echo "unknown")
  
  echo "--- Device: $DEVICE (from $diagdir) ---"
  
  # Check for telldus-mqtt logs
  LOGFILE=""
  if [ -f "$diagdir/logread.telldus-mqtt" ]; then
    LOGFILE="$diagdir/logread.telldus-mqtt"
  elif [ -f "$diagdir/logread.full" ]; then
    LOGFILE="$diagdir/logread.full"
  fi
  
  if [ -z "$LOGFILE" ] || [ ! -f "$LOGFILE" ]; then
    echo "  No logs found"
    echo
    continue
  fi
  
  # Count restart categories
  TELLDUS_LOST=$(grep -E "Telldus error|Could not connect to the Telldus|telldus.*connection.*lost" "$LOGFILE" 2>/dev/null | wc -l)
  MQTT_LOST=$(grep -E "MQTT is not connected|mosquitto.*connection.*lost|Connection Accepted" "$LOGFILE" 2>/dev/null | wc -l)
  WATCHDOG=$(grep -E "telldus-mqtt-watchdog.sh: Message timeout" "$LOGFILE" 2>/dev/null | wc -l)
  
  # Show counts
  echo "  Telldusd connection issues: $TELLDUS_LOST"
  echo "  MQTT connection issues: $MQTT_LOST"
  echo "  Watchdog restarts: $WATCHDOG"
  
  # Show recent relevant log lines
  if [ "$TELLDUS_LOST" -gt 0 ]; then
    echo "  Recent telldusd errors:"
    grep -i "telldus error\|could not connect to the telldus" "$LOGFILE" 2>/dev/null | tail -n 3 | sed 's/^/    /' || true
  fi
  
  if [ "$MQTT_LOST" -gt 0 ]; then
    echo "  Recent MQTT errors:"
    grep -i "mqtt is not connected\|mosquitto.*connection" "$LOGFILE" 2>/dev/null | tail -n 3 | sed 's/^/    /' || true
  fi
  
  if [ "$WATCHDOG" -gt 0 ]; then
    echo "  Watchdog events:"
    grep -i "watchdog" "$LOGFILE" 2>/dev/null | tail -n 3 | sed 's/^/    /' || true
  fi
  
  echo
done

echo "=== SUMMARY ==="
echo
echo "To review full logs:"
echo "  cd $COLLECTED_DIR"
echo "  grep -r 'telldus-mqtt' telldus-mqtt-diag-*/logread.*"
echo
