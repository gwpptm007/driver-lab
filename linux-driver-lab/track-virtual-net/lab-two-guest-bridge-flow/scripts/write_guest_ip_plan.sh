#!/usr/bin/env bash
set -euo pipefail
OUT=${1:-guest_ip_plan.txt}
cat > "$OUT" <<'EOF'
# guest A
ip addr add 192.168.100.2/24 dev eth0
ip link set eth0 up
ip addr
ping -c 5 192.168.100.3

# guest B
ip addr add 192.168.100.3/24 dev eth0
ip link set eth0 up
ip addr
ping -c 5 192.168.100.2
EOF
echo "$OUT"
