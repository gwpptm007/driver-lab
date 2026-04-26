#!/usr/bin/env bash
set -euo pipefail
DIR=$(cd -- "$(dirname -- "$0")" && pwd)
# shellcheck source=common.sh
source "$DIR/common.sh"

echo "[*] checking environment for lab-virtio-net-ethtool-stats-mini-patch"

for cmd in ethtool ip uname date diff grep; do
    need_cmd "$cmd"
done

echo "[*] kernel src: $(default_kernel_src)"
if [[ -d "$(default_kernel_src)" ]]; then
    echo "[ok] kernel source exists"
else
    echo "[warn] kernel source path not found: $(default_kernel_src)"
fi

echo "[*] note: this lab is intended for a real virtio_net experiment environment"
echo "[*] first confirm interface driver with: ethtool -i <ifname>"
