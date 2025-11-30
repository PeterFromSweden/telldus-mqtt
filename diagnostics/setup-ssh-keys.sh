#!/bin/sh
# setup-ssh-keys.sh
# Simple, POSIX-compatible SSH key provisioning for OpenWrt devices.
#
# Features:
# - Generates a dedicated keypair at ~/.ssh/diag_collector (ed25519) if missing
# - Tries to install the public key on each host using (in order):
#     1) passwordless SSH (already authorized)
#     2) ssh-copy-id (interactive password prompt)
#     3) sshpass + ssh-copy-id if SSH_PASSWORD is set and sshpass exists
#     4) fallback: prints the public key and a one-line command to add it on the device
#
# Usage:
#   ./setup-ssh-keys.sh [host1 host2 ...]
# Environment:
#   SSH_USER (default: root)
#   SSH_KEY (default: ~/.ssh/diag_collector)
#   SSH_PASSWORD (optional - used with sshpass for non-interactive provisioning)

set -eu

SSH_USER=${SSH_USER:-root}
SSH_KEY=${SSH_KEY:-$HOME/.ssh/diag_collector}
PUBKEY="${SSH_KEY}.pub"

# Default devices (can be overridden by passing hosts as arguments)
DEFAULT_HOSTS="10.0.0.10 10.0.0.23 10.0.0.12 192.168.12.228 192.168.9.1"

usage() {
  echo "Usage: $0 [host1 host2 ...]"
  echo "Env: SSH_USER, SSH_KEY, SSH_PASSWORD"
  exit 1
}

if [ "$#" -eq 0 ]; then
  HOSTS="$DEFAULT_HOSTS"
else
  HOSTS="$*"
fi

# Ensure .ssh dir
mkdir -p "$(dirname "$SSH_KEY")" || true

# Generate key if missing
if [ ! -f "$SSH_KEY" ]; then
  echo "Generating SSH key at $SSH_KEY"
  ssh-keygen -t ed25519 -f "$SSH_KEY" -N "" || {
    echo "ssh-keygen failed" >&2
    exit 2
  }
fi

if [ ! -f "$PUBKEY" ]; then
  echo "Public key not found at $PUBKEY" >&2
  exit 3
fi

echo "Using key: $SSH_KEY (user=${SSH_USER})"

for host in $HOSTS; do
  echo "--- $host ---"

  # Quick check: can we connect without password?
  if ssh -i "$SSH_KEY" -o BatchMode=yes -o ConnectTimeout=5 -l "$SSH_USER" "$host" 'echo ok' >/dev/null 2>&1; then
    echo "OK: passwordless SSH already works on $host"
    continue
  fi

  # Try ssh-copy-id (interactive)
  if command -v ssh-copy-id >/dev/null 2>&1; then
    echo "Attempting ssh-copy-id to $SSH_USER@$host (may prompt for password)"
    ssh-copy-id -i "$PUBKEY" -o StrictHostKeyChecking=no "$SSH_USER@$host" && { echo "Installed key on $host"; continue; } || true
  fi

  # Try non-interactive via sshpass if SSH_PASSWORD supplied
  if [ -n "${SSH_PASSWORD:-}" ] && command -v sshpass >/dev/null 2>&1 && command -v ssh-copy-id >/dev/null 2>&1; then
    echo "Using sshpass to install key on $host"
    sshpass -p "$SSH_PASSWORD" ssh-copy-id -i "$PUBKEY" -o StrictHostKeyChecking=no "$SSH_USER@$host" && { echo "Installed key on $host"; continue; } || true
  fi

  # Fallback: print instructions
  echo "Could not install key automatically on $host. Manual steps:" 
  echo "  1) SSH to the device as root and create ~/.ssh if missing"
  echo "  2) Append this public key to ~/.ssh/authorized_keys on the device"
  echo
  echo "Public key (BEGIN)"
  sed -n '1,200p' "$PUBKEY" || true
  echo "Public key (END)"
  echo "On the device run: mkdir -p ~/.ssh && echo '<paste-key-line>' >> ~/.ssh/authorized_keys && chmod 600 ~/.ssh/authorized_keys"
  echo
done

echo "Done"
