#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0
# build_xdp.sh — 编译 stage14 XDP 示例 BPF 程序
#
# 用法：
#   ./build_xdp.sh              # 编译全部
#   ./build_xdp.sh xdp_pass     # 只编译 xdp_pass_kern.o
#   ./build_xdp.sh xdp_drop     # 只编译 xdp_drop_kern.o
#
# 依赖：
#   - clang (>= 10)
#   - llvm-strip, llc, llvm-objcopy (通常随 llvm 包)
#   - kernel headers: /usr/include/bpf 或 $KDIR/tools/lib/bpf
#
# 输出：
#   xdp_pass_kern.o  — XDP_PASS 示例（所有包上送协议栈）
#   xdp_drop_kern.o  — XDP_DROP 示例（所有包丢弃）
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BPF_DIR="$SCRIPT_DIR"
KDIR="${KDIR:-/lib/modules/$(uname -r)/build}"

# 颜色
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo_warning() { echo -e "${YELLOW}[WARN]${NC} $*"; }
echo_success() { echo -e "${GREEN}[PASS]${NC} $*"; }
echo_fail() { echo -e "${RED}[FAIL]${NC} $*" >&2; }

# 检查 clang
check_clang() {
    if ! command -v clang &>/dev/null; then
        echo_fail "clang 未找到，请安装: apt install clang llvm"
        return 1
    fi
    local ver
    ver=$(clang --version | head -1)
    echo "  clang: $ver"
    return 0
}

# 检查 kernel bpf headers
check_bpf_h() {
    local hdrs=("/usr/include/bpf" "$KDIR/tools/lib/bpf" "$KDIR/include")

    for h in "${hdrs[@]}"; do
        if [[ -f "$h/bpf_helpers.h" ]] || [[ -f "$h/bpf/bpf_helpers.h" ]]; then
            echo "  BPF headers: $h"
            return 0
        fi
    done

    echo_warning "未找到 bpf_helpers.h，编译可能失败"
    return 0
}

# 编译单个 XDP program
# 参数: $1 = 源文件, $2 = 输出文件, $3 = section name
compile_one() {
    local src="$1"
    local out="$2"
    local sec="$3"

    echo "  编译 $src -> $out"

    # clang -O2 -target bpf: 优化级别 O2，生成 BPF 目标文件
    # -Wall: 开启所有警告
    # -I: 添加 include 路径
    # Ubuntu 上的 BPF headers 位于内核源码树：
    #   /usr/src/linux-headers-VERSION/tools/bpf/resolve_btfids/libbpf/include/
    # asm/types.h 在 /usr/include/x86_64-linux-gnu/asm/
    local kver=$(uname -r)
    local inc_flags="-I /usr/src/linux-headers-${kver}/tools/bpf/resolve_btfids/libbpf/include -I /usr/include/linux -I /usr/include/asm-generic -I /usr/include/x86_64-linux-gnu"

    clang -O2 -target bpf -Wall -g $inc_flags -c "$src" -o "$out" 2>&1 | while read line; do
        echo "    $line"
    done

    if [[ ! -f "$out" ]]; then
        echo_fail "编译失败: $out 未生成"
        return 1
    fi

    # 验证 BPF section 存在
    if command -v llvm-objdump &>/dev/null; then
        echo "    section 信息:"
        llvm-objdump -h "$out" 2>/dev/null | grep -E "Idx|Name" || true
    fi

    echo_success "  $out 生成成功"
    return 0
}

# 主编译逻辑
build_all() {
    echo "========================================="
    echo "build_xdp: 编译 stage14 XDP 示例程序"
    echo "========================================="
    echo ""
    echo "检查工具链..."
    check_clang || true
    check_bpf_h || true
    echo ""

    echo "编译 XDP 程序..."
    echo ""

    compile_one \
        "$BPF_DIR/xdp_pass_kern.c" \
        "$BPF_DIR/xdp_pass_kern.o" \
        "xdp_pass" || return 1

    echo ""

    compile_one \
        "$BPF_DIR/xdp_drop_kern.c" \
        "$BPF_DIR/xdp_drop_kern.o" \
        "xdp_drop" || return 1

    echo ""
    echo_success "========================================="
    echo_success "全部编译完成！"
    echo_success "========================================="
    echo ""
    echo "使用示例："
    echo "  # 加载 XDP_PASS（所有包上送协议栈）"
    echo "  sudo ip link set dev nds14s xdp obj xdp_pass_kern.o sec xdp_pass"
    echo ""
    echo "  # 加载 XDP_DROP（所有包丢弃，tcpdump 看不到）"
    echo "  sudo ip link set dev nds14s xdp obj xdp_drop_kern.o sec xdp_drop"
    echo ""
    echo "  # 查看统计"
    echo "  ethtool -S nds14s | grep xdp_"
    echo ""
    echo "  # 卸载"
    echo "  sudo ip link set dev nds14s xdp off"
    echo ""
}

build_one() {
    local name="$1"
    local src out sec

    case "$name" in
        xdp_pass)
            src="$BPF_DIR/xdp_pass_kern.c"
            out="$BPF_DIR/xdp_pass_kern.o"
            sec="xdp_pass"
            ;;
        xdp_drop)
            src="$BPF_DIR/xdp_drop_kern.c"
            out="$BPF_DIR/xdp_drop_kern.o"
            sec="xdp_drop"
            ;;
        *)
            echo_fail "未知目标: $name"
            echo "可用: xdp_pass, xdp_drop"
            return 1
            ;;
    esac

    if [[ ! -f "$src" ]]; then
        echo_fail "源文件不存在: $src"
        return 1
    fi

    compile_one "$src" "$out" "$sec"
}

# 入口
TARGET="${1:-all}"

if [[ "$TARGET" == "all" ]]; then
    build_all
else
    build_one "$TARGET"
fi