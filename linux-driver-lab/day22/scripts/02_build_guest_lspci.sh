#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=/dev/null
source "${SCRIPT_DIR}/common.sh"

# 说明：
#   1. day22 强依赖 guest 里能跑 lspci。
#   2. 由于宿主机通常是 x86_64，而 guest 是 arm64，所以不能直接把宿主机 /usr/bin/lspci 拷进 guest。
#   3. 这里提供一个“尽量帮你构建 arm64 静态 lspci”的辅助脚本。
#   4. 如果你的环境里已经有 arm64 静态 lspci，最省事的方法仍是直接设置 GUEST_LSPCI_BIN。

if [[ -x "${GUEST_LSPCI_BIN}" ]]; then
    if is_elf_aarch64_static "${GUEST_LSPCI_BIN}"; then
        log "已有可用的 arm64 静态 lspci，跳过构建：${GUEST_LSPCI_BIN}"
        exit 0
    fi
    warn "${GUEST_LSPCI_BIN} 已存在，但不是 arm64 静态 ELF；仍尝试重新构建。"
fi

[[ -d "${PCIUTILS_SRC_DIR}" ]] || die "找不到 pciutils 源码目录：${PCIUTILS_SRC_DIR}"
require_cmd "${CC}"
require_cmd "${AR}"
require_cmd "${RANLIB}"

ensure_dir "$(dirname "${GUEST_LSPCI_BIN}")"

log "尝试交叉编译 arm64 静态 lspci"
log "源码目录：${PCIUTILS_SRC_DIR}"
log "输出路径：${GUEST_LSPCI_BIN}"

# 这里采用 best-effort 方式构建。
# 目标是尽量减少额外依赖，因此关闭 DNS / ZLIB 等可选项，并尝试静态链接。
# 如果你的工具链缺少静态 libc，构建可能失败；这时请直接准备一个预编译的 arm64 静态 lspci。
(
    cd "${PCIUTILS_SRC_DIR}"
    make clean >/dev/null 2>&1 || true
    make \
        HOST=linux \
        CC="${CC}" \
        AR="${AR}" \
        RANLIB="${RANLIB}" \
        STRIP="${STRIP}" \
        DNS=no \
        ZLIB=no \
        SHARED=no \
        HWDB=no \
        CFLAGS='-O2 -static' \
        LDFLAGS='-static' \
        lspci
)

cp -f "${PCIUTILS_SRC_DIR}/lspci" "${GUEST_LSPCI_BIN}"
chmod +x "${GUEST_LSPCI_BIN}"

if is_elf_aarch64_static "${GUEST_LSPCI_BIN}"; then
    log "构建成功：${GUEST_LSPCI_BIN}"
else
    die "lspci 构建结束，但产物不是预期的 arm64 静态 ELF，请检查工具链或直接提供预编译二进制。"
fi
