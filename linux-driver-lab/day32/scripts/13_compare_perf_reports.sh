#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
rec="${RECORDS_DIR}/${RUN_ID}"
out="${OUTPUT_DIR}/day32_perf_summary.md"
ensure_dir "${OUTPUT_DIR}"

require_file "$rec/bench-mmap-baseline.txt" bench-mmap-baseline.txt
require_file "$rec/bench-mmap-optimized.txt" bench-mmap-optimized.txt

# 这里优先复用 records 中已经归档好的 baseline/optimized bench 结果，
# 即使宿主 perf 没跑，这个脚本也能产出“优化前后”摘要。
get_val() {
  local file="$1" key="$2"
  awk -F= -v k="$key" '$1==k {print $2; exit}' "$file"
}
base_avg="$(get_val "$rec/bench-mmap-baseline.txt" avg_us)"
opt_avg="$(get_val "$rec/bench-mmap-optimized.txt" avg_us)"
base_p99="$(get_val "$rec/bench-mmap-baseline.txt" p99_us)"
opt_p99="$(get_val "$rec/bench-mmap-optimized.txt" p99_us)"
base_tp="$(get_val "$rec/bench-mmap-baseline.txt" throughput_mbps)"
opt_tp="$(get_val "$rec/bench-mmap-optimized.txt" throughput_mbps)"

python3 - "$base_avg" "$opt_avg" "$base_p99" "$opt_p99" "$base_tp" "$opt_tp" "$out" <<'PY'
import sys
base_avg=float(sys.argv[1]); opt_avg=float(sys.argv[2]); base_p99=float(sys.argv[3]); opt_p99=float(sys.argv[4]); base_tp=float(sys.argv[5]); opt_tp=float(sys.argv[6]); out=sys.argv[7]
lat_gain=(base_avg-opt_avg)/base_avg*100 if base_avg else 0.0
p99_gain=(base_p99-opt_p99)/base_p99*100 if base_p99 else 0.0
thr_gain=(opt_tp-base_tp)/base_tp*100 if base_tp else 0.0
text=f'''# Day32 Perf Summary\n\n- baseline avg_us: {base_avg:.3f}\n- optimized avg_us: {opt_avg:.3f}\n- avg latency gain: {lat_gain:.2f}%\n- baseline p99_us: {base_p99:.3f}\n- optimized p99_us: {opt_p99:.3f}\n- p99 latency gain: {p99_gain:.2f}%\n- baseline throughput_mbps: {base_tp:.3f}\n- optimized throughput_mbps: {opt_tp:.3f}\n- throughput gain: {thr_gain:.2f}%\n\n## Interpretation\n- baseline 模式每轮都会重新 GET_INFO + mmap + munmap，热路径里 syscall 和 VMA 建立/销毁占比更高。\n- optimized 模式把 layout 查询和 mmap 固定到循环外，减少了重复系统调用与地址空间管理成本。\n- Day32 的最小优化点，就是把“每轮重复准备”收敛为“bench 前准备一次，循环内只做核心数据路径”。\n'''
open(out,'w').write(text)
PY

echo "[day32] perf 对比摘要已生成：$out"
cat "$out"
