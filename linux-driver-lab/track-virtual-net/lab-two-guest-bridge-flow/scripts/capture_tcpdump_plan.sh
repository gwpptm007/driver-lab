#!/usr/bin/env bash
set -euo pipefail

TAPA=${TAPA:-tap-vnet-a}
TAPB=${TAPB:-tap-vnet-b}
BR=${BR:-br-vnet0}

cat <<EOF
# Run these manually while guest A is pinging guest B.

sudo tcpdump -i $TAPA -n -e icmp -c 10
sudo tcpdump -i $BR   -n -e icmp -c 10
sudo tcpdump -i $TAPB -n -e icmp -c 10

# Suggested output files:
# tcpdump_tap_a_icmp.txt
# tcpdump_bridge_icmp.txt
# tcpdump_tap_b_icmp.txt
EOF
