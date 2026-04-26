#!/usr/bin/env bash
set -euo pipefail
IFNAME=${1:?usage: $0 <ifname> <peer-ip> [count]}
PEER_IP=${2:?usage: $0 <ifname> <peer-ip> [count]}
COUNT=${3:-20}
ping -I "$IFNAME" -c "$COUNT" "$PEER_IP"
