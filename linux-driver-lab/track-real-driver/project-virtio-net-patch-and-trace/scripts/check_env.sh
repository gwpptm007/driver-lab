#!/usr/bin/env bash
set -euo pipefail
DIR=$(cd -- "$(dirname -- "$0")" && pwd)
# shellcheck source=common.sh
source "$DIR/common.sh"

echo "[*] checking environment for project-virtio-net-patch-and-trace"
for cmd in ethtool ip uname date diff grep; do
    need_cmd "$cmd"
done
echo "[*] project expects a virtio_net-capable guest environment"
echo "[*] verify interface with: ethtool -i <ifname>"
