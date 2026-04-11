#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=/dev/null
source "${SCRIPT_DIR}/common.sh"

TMP_ROOT="${WORKDIR}/selftest-pci-sysfs"
GUEST_BIN="${TOOLS_DIR}/aarch64/pci_sysfs_dump"
HOST_BIN_DIR="${TOOLS_DIR}/host"
HOST_BIN="${HOST_BIN_DIR}/pci_sysfs_dump"
OUT="${WORKDIR}/selftest-pci-sysfs.out"
SRC="${DAY22_ROOT}/tools/pci_sysfs_dump.c"

log "开始执行 pci_sysfs_dump 自测"
rm -rf "${TMP_ROOT}" "${OUT}"
mkdir -p "${TMP_ROOT}/0000:00:01.0" "${HOST_BIN_DIR}"

cat > "${TMP_ROOT}/0000:00:01.0/vendor" <<'EOT'
0x1af4
EOT
cat > "${TMP_ROOT}/0000:00:01.0/device" <<'EOT'
0x1110
EOT
cat > "${TMP_ROOT}/0000:00:01.0/class" <<'EOT'
0x050000
EOT
cat > "${TMP_ROOT}/0000:00:01.0/irq" <<'EOT'
35
EOT
cat > "${TMP_ROOT}/0000:00:01.0/resource" <<'EOT'
0x0000000040000000 0x00000000400000ff 0x0000000000000200
0x0000000041000000 0x00000000413fffff 0x0000000000000200
EOT
python3 - <<'PY' > "${TMP_ROOT}/0000:00:01.0/config"
import sys
sys.stdout.buffer.write(bytes(range(64)))
PY

[[ -x "${GUEST_BIN}" ]] || die "请先执行 make build-tools，未找到 guest 工具：${GUEST_BIN}"
require_cmd "${HOST_CC}"

log "检测到 guest 工具是 arm64 程序，宿主机自测将改用 host 版临时二进制"
log "HOST_CC=${HOST_CC}"
"${HOST_CC}" -O2 -Wall -Wextra -o "${HOST_BIN}" "${SRC}"
"${HOST_STRIP}" "${HOST_BIN}" >/dev/null 2>&1 || true
require_executable_file "${HOST_BIN}"

PCI_SYSFS_ROOT="${TMP_ROOT}" "${HOST_BIN}" > "${OUT}"

grep -q '^\[device\] 0000:00:01.0$' "${OUT}" || die "自测失败：未打印设备 BDF"
grep -q 'vendor   : 0x1af4' "${OUT}" || die "自测失败：未打印 vendor"
grep -q 'device   : 0x1110' "${OUT}" || die "自测失败：未打印 device"
grep -q '^# total devices: 1$' "${OUT}" || die "自测失败：设备计数不正确"

log "pci_sysfs_dump 自测通过：${OUT}"
log "guest arm64 工具保留在：${GUEST_BIN}"
log "host 自测工具保留在：${HOST_BIN}"
