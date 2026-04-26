#!/usr/bin/env bash
set -euo pipefail

BR=${BR:-br-vnet0}
TAPA=${TAPA:-tap-vnet-a}
TAPB=${TAPB:-tap-vnet-b}
HOST_IP=${HOST_IP:-192.168.100.1/24}
USER_NAME=${USER_NAME:-$(whoami)}

cat <<EOF
# Dry-run plan. Review before executing manually.

sudo modprobe tun
sudo modprobe bridge

sudo ip link add name $BR type bridge
sudo ip addr add $HOST_IP dev $BR
sudo ip link set $BR up

sudo ip tuntap add dev $TAPA mode tap user $USER_NAME
sudo ip link set $TAPA master $BR
sudo ip link set $TAPA up

sudo ip tuntap add dev $TAPB mode tap user $USER_NAME
sudo ip link set $TAPB master $BR
sudo ip link set $TAPB up

# cleanup:
# sudo ip link set $TAPA down
# sudo ip link del $TAPA
# sudo ip link set $TAPB down
# sudo ip link del $TAPB
# sudo ip link set $BR down
# sudo ip link del $BR
EOF
