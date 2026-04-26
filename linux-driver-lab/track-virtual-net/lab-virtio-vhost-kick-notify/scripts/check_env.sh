#!/usr/bin/env bash
set -euo pipefail
DIR=$(cd -- "$(dirname -- "$0")" && pwd)
# shellcheck source=common.sh
source "$DIR/common.sh"

say "checking tools"
for cmd in ip bridge qemu-system-aarch64 tcpdump ps grep awk sed diff; do
    if command -v "$cmd" >/dev/null 2>&1; then
        echo "[ok] $cmd"
    else
        echo "[warn] missing $cmd"
    fi
done

say "checking tap/tun/vhost"
[[ -e /dev/net/tun ]] && echo "[ok] /dev/net/tun exists" || echo "[warn] /dev/net/tun missing"
[[ -e /dev/vhost-net ]] && echo "[ok] /dev/vhost-net exists" || echo "[warn] /dev/vhost-net missing"

say "loaded modules"
lsmod | grep -E 'vhost|tun|bridge' || true

cat <<'EOF'

If vhost_net is missing, try:
  sudo modprobe vhost_net

If tap/bridge topology is missing, finish:
  ../lab-virtio-tap-bridge-path/
EOF
