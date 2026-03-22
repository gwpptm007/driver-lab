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
log "PCIUTILS_HOST：${PCIUTILS_HOST}"

# 这里采用 best-effort 方式构建。
# 关键点：pciutils/lib/configure 在 HOST 未指定时会按宿主机 uname -m 探测 CPU。
# 如果宿主机是 x86_64，就会把 i386-ports 也编进去，交叉到 aarch64 时常见报错是缺少 <sys/io.h>。
# 因此这里必须显式传 HOST=aarch64-linux-gnu，再配合 CROSS_COMPILE=aarch64-linux-gnu-。
# 另外尽量关闭 DNS / ZLIB / HWDB 等可选项，并优先尝试静态链接。
# 如果你的工具链缺少静态 libc，构建可能失败；这时请先去掉静态链接参数，只要求生成 arm64 lspci。
(
    cd "${PCIUTILS_SRC_DIR}"
    make clean >/dev/null 2>&1 || true
    make \
        HOST="${PCIUTILS_HOST}" \
        CROSS_COMPILE="${CROSS_COMPILE}" \
        CC="${CC}" \
        AR="${AR}" \
        RANLIB="${RANLIB}" \
        STRIP="${STRIP}" \
        DNS=no \
        ZLIB=no \
        SHARED=no \
        HWDB=no \
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

log "如果这里仍然失败，请优先查看 docs/02_PREPARE_ENV_AND_LSPCI.md 中的常见报错章节。"
