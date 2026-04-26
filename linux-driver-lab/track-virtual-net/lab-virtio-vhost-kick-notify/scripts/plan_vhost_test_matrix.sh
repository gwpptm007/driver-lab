#!/usr/bin/env bash
set -euo pipefail
cat <<'EOF'
# vhost test matrix

## Round 1: vhost=off
QEMU args:
  ./scripts/generate_qemu_vhost_args.sh off

Guest:
  ip addr add 192.168.100.2/24 dev eth0
  ip link set eth0 up
  ping -c 5 192.168.100.1

Host:
  ./scripts/collect_mode_state.sh off <record-dir>

## Round 2: vhost=on
QEMU args:
  ./scripts/generate_qemu_vhost_args.sh on

Guest:
  ip addr add 192.168.100.2/24 dev eth0
  ip link set eth0 up
  ping -c 5 192.168.100.1

Host:
  ./scripts/collect_mode_state.sh on <record-dir>

## Compare:
  ./scripts/diff_vhost_modes.sh <record-dir>
EOF
