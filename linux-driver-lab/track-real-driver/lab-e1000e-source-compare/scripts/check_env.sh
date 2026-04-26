#!/usr/bin/env bash
set -euo pipefail
DIR=$(cd -- "$(dirname -- "$0")" && pwd)
# shellcheck source=common.sh
source "$DIR/common.sh"

echo "[*] checking environment for lab-e1000e-source-compare"
for cmd in ethtool lspci grep ip; do
    need_cmd "$cmd"
done

echo "[*] verify a candidate interface with:"
echo "    ethtool -i <ifname>"
echo "    lspci -nnk | grep -A 3 -i ethernet"
