#!/usr/bin/env bash
set -euo pipefail
IFNAME=${1:?usage: $0 <ifname> <server-ip> [seconds]}
SERVER_IP=${2:?usage: $0 <ifname> <server-ip> [seconds]}
SECONDS=${3:-10}
iperf3 -B "$IFNAME" -c "$SERVER_IP" -t "$SECONDS"
