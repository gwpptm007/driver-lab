#!/usr/bin/env bash
set -euo pipefail
IFNAME=${1:?usage: $0 <ifname> <out-prefix>}
OUT_PREFIX=${2:?usage: $0 <ifname> <out-prefix>}

ethtool -i "$IFNAME" > "${OUT_PREFIX}_ethtool_i.txt" 2>&1 || true
ethtool -S "$IFNAME" > "${OUT_PREFIX}_ethtool_S.txt" 2>&1 || true
ip -s link show "$IFNAME" > "${OUT_PREFIX}_ip_link.txt" 2>&1 || true
