#!/bin/sh
set -u

#
# 这个脚本做“最后一道保底检查”：
# 功能脚本通过不代表内核里一定没问题
#
# 有些 bug 表面上用户态没报错，但 dmesg 已经出现：
# - Oops
# - Call Trace
# - KASAN/UBSAN
# - use-after-free
# - memory leak / unreferenced object
#
# 所以 Day06 的验收一定要把 dmesg 扫一遍
#
PATTERN='Oops:|BUG:|Call Trace:|KASAN:|UBSAN:|general protection fault|slab-out-of-bounds|use-after-free|memory leak|unreferenced object|leaked'
OUT=/tmp/dmesg_scan.txt

echo "[day06] scanning dmesg for suspicious patterns"

if dmesg | grep -Ei "$PATTERN" >"$OUT" 2>&1; then
    echo "[FAIL] suspicious dmesg pattern found"
    cat "$OUT"
    exit 1
fi

echo "[PASS] no suspicious dmesg pattern found"
exit 0
