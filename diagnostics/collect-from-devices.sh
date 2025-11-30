#!/bin/sh
# collect-from-devices.sh
# Orchestrator that runs collect-telldus-mqtt.sh on multiple OpenWrt devices
# and fetches the diagnostic tarballs back to a local directory.
#
# Usage:
#   ./collect-from-devices.sh [host1 host2 ...]
# Environment:
#   SSH_USER (default: root)
#   SSH_KEY (default: ~/.ssh/diag_collector)
#   OUTPUT_DIR (default: ./collected-YYYYMMDD-HHMMSS)

set -eu

SSH_USER=${SSH_USER:-root}
SSH_KEY=${SSH_KEY:-$HOME/.ssh/diag_collector}
COLLECTOR_SCRIPT="$(dirname "$0")/collect-telldus-mqtt.sh"
REMOTE_SCRIPT="/tmp/collect-telldus-mqtt.sh"

# Default devices
DEFAULT_HOSTS="10.0.0.10 10.0.0.23 10.0.0.12 192.168.12.228 192.168.9.1"

if [ "$#" -eq 0 ]; then
  HOSTS="$DEFAULT_HOSTS"
else
  HOSTS="$*"
fi

if [ ! -f "$COLLECTOR_SCRIPT" ]; then
  echo "Collector script not found: $COLLECTOR_SCRIPT" >&2
  exit 1
fi

if [ ! -f "$SSH_KEY" ]; then
  echo "SSH key not found: $SSH_KEY" >&2
  echo "Run setup-ssh-keys.sh first" >&2
  exit 2
fi

# Create output directory
OUTPUT_DIR=${OUTPUT_DIR:-./collected-$(date +%Y%m%d-%H%M%S)}
mkdir -p "$OUTPUT_DIR"

echo "Collecting diagnostics from devices..."
echo "Output directory: $OUTPUT_DIR"
echo

for host in $HOSTS; do
  echo "=== $host ==="
  
  # Copy collector script to device
  if ! scp -i "$SSH_KEY" -o StrictHostKeyChecking=no "$COLLECTOR_SCRIPT" "$SSH_USER@$host:$REMOTE_SCRIPT" 2>/dev/null; then
    echo "Failed to copy collector to $host"
    continue
  fi
  
  # Run collector and capture output
  OUTPUT=$(ssh -i "$SSH_KEY" -o StrictHostKeyChecking=no "$SSH_USER@$host" "sh $REMOTE_SCRIPT" 2>/dev/null || true)
  
  # Extract tarball path from output (last line before "done")
  TARBALL=$(echo "$OUTPUT" | grep -E '^/tmp/telldus-mqtt-diag-.*\.tar\.gz$' | tail -n1)
  
  if [ -z "$TARBALL" ]; then
    echo "Collector failed or no tarball path found on $host"
    echo "$OUTPUT"
    continue
  fi
  
  echo "Tarball created: $TARBALL"
  
  # Fetch tarball
  LOCAL_NAME="$(basename "$TARBALL" .tar.gz)-${host}.tar.gz"
  if ! scp -i "$SSH_KEY" -o StrictHostKeyChecking=no "$SSH_USER@$host:$TARBALL" "$OUTPUT_DIR/$LOCAL_NAME" 2>/dev/null; then
    echo "Failed to fetch tarball from $host"
    continue
  fi
  
  # Fetch checksum if available
  scp -i "$SSH_KEY" -o StrictHostKeyChecking=no "$SSH_USER@$host:${TARBALL}.sha256" "$OUTPUT_DIR/${LOCAL_NAME}.sha256" 2>/dev/null || true
  
  # Verify checksum if fetched
  if [ -f "$OUTPUT_DIR/${LOCAL_NAME}.sha256" ]; then
    # Rewrite checksum file with local filename
    CHECKSUM=$(awk '{print $1}' "$OUTPUT_DIR/${LOCAL_NAME}.sha256")
    echo "$CHECKSUM  $LOCAL_NAME" > "$OUTPUT_DIR/${LOCAL_NAME}.sha256"
    if (cd "$OUTPUT_DIR" && sha256sum -c "${LOCAL_NAME}.sha256" 2>/dev/null); then
      echo "Checksum OK"
    else
      echo "Checksum FAILED!"
    fi
  fi
  
  # Cleanup remote files
  ssh -i "$SSH_KEY" -o StrictHostKeyChecking=no "$SSH_USER@$host" "rm -f $TARBALL ${TARBALL}.sha256 $REMOTE_SCRIPT" 2>/dev/null || true
  
  echo "Collected: $OUTPUT_DIR/$LOCAL_NAME"
  echo
done

echo "Done. Diagnostics collected to: $OUTPUT_DIR"
echo
echo "To extract and review:"
echo "  cd $OUTPUT_DIR"
echo "  for f in *.tar.gz; do tar xzf \$f; done"
