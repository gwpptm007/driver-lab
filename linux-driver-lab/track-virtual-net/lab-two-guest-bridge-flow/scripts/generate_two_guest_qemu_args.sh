#!/usr/bin/env bash
set -euo pipefail

TAPA=${TAPA:-tap-vnet-a}
TAPB=${TAPB:-tap-vnet-b}
MACA=${MACA:-52:54:00:12:34:a1}
MACB=${MACB:-52:54:00:12:34:b1}
VHOST=${VHOST:-off}

cat <<EOF
# guest A:
-netdev tap,id=net0,ifname=$TAPA,script=no,downscript=no,vhost=$VHOST \\
-device virtio-net-pci,netdev=net0,mac=$MACA

# guest B:
-netdev tap,id=net0,ifname=$TAPB,script=no,downscript=no,vhost=$VHOST \\
-device virtio-net-pci,netdev=net0,mac=$MACB
EOF
