#!/usr/bin/env bash
set -euo pipefail
IFACE=${1:-tap-vnet0}
cat <<EOF
# Run manually in another terminal while ping is running:

sudo tcpdump -i $IFACE -n -e icmp -c 10

# For bridge:
sudo tcpdump -i br-vnet0 -n -e icmp -c 10
EOF
