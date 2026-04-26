#!/usr/bin/env bash
set -euo pipefail
BR=${BR:-br-vnet0}
TAP=${TAP:-tap-vnet0}
HOST_IP=${HOST_IP:-192.168.100.1/24}

cat <<EOF
# Dry-run plan. Review before executing manually.

sudo modprobe tun
sudo modprobe bridge

sudo ip link add name $BR type bridge
sudo ip addr add $HOST_IP dev $BR
sudo ip link set $BR up

sudo ip tuntap add dev $TAP mode tap user \$(whoami)
sudo ip link set $TAP master $BR
sudo ip link set $TAP up

# cleanup:
# sudo ip link set $TAP down
# sudo ip link del $TAP
# sudo ip link set $BR down
# sudo ip link del $BR
EOF
