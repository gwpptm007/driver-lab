#!/usr/bin/env bash
set -euo pipefail
DIR=$(cd -- "$(dirname -- "$0")" && pwd)
# shellcheck source=common.sh
source "$DIR/common.sh"

echo "[*] checking environment for lab-virtio-net-queue-poll-observe"
for cmd in ethtool ip uname date diff grep; do
    need_cmd "$cmd"
done
echo "[*] this lab expects a virtio_net-capable guest environment"
echo "[*] verify with: ethtool -i <ifname>"
