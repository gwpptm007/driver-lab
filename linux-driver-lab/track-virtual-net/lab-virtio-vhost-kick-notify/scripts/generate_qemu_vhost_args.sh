#!/usr/bin/env bash
set -euo pipefail
MODE=${1:-on}
TAP=${TAP:-tap-vnet0}
MAC=${MAC:-52:54:00:12:34:56}

case "$MODE" in
    on|off) ;;
    *) echo "usage: $0 <on|off>" >&2; exit 1 ;;
esac

cat <<EOF
-netdev tap,id=net0,ifname=$TAP,script=no,downscript=no,vhost=$MODE \\
-device virtio-net-pci,netdev=net0,mac=$MAC
EOF
