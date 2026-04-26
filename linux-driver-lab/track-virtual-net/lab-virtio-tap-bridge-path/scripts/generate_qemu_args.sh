#!/usr/bin/env bash
set -euo pipefail
TAP=${TAP:-tap-vnet0}
cat <<EOF
-netdev tap,id=net0,ifname=$TAP,script=no,downscript=no \\
-device virtio-net-pci,netdev=net0
EOF
