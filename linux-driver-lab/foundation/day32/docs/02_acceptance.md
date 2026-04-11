# day32 验收清单

## 本轮验收结论

基于 `records/day32-local-001`，**day32 默认主链路验收通过**。

## 必须满足

- [x] EDU 枚举通过
- [x] 驱动 probe 通过
- [x] `mmap-verify` 通过
- [x] `bench-mmap-baseline` 有有效结果
- [x] `bench-mmap`（optimized）有有效结果
- [x] `compare-mmap` 形成前后对比
- [x] guest 流程完整结束
- [x] 无 panic / oops / DMA mapping error

## 建议额外补充

- [x] `bench-ioctl` 作为控制路径对照已留证
- [x] `bench-dma-lite` 作为设备路径对照已留证
- [ ] 宿主 `perf stat` / `perf report` 已至少保留一份

> 说明：宿主 perf 这轮未采集到，不影响 day32 默认主链路通过；它属于“热点证据增强项”，不是本轮阻断项。

## 当前 records 对应的关键证据

### 功能与稳定性

- `mmap-verify.txt`
  - `verify_ok=1`
  - `run_ok=1`
  - `irq_delta=2`
  - `mmap_ok=1`
- `run-result.txt`
  - `total_run_calls=37`
  - `total_run_ok=37`
  - `total_run_fail=0`
- `run-summary.md`
  - `guest flow complete: yes`
  - `qemu timeout hit: no`
  - `oops/dma-error/hung/panic found: no`

### baseline vs optimized

- `bench-mmap-baseline.txt`
  - `avg_us=283.590`
  - `p99_us=641.344`
  - `throughput_mbps=6.585`
- `bench-mmap-optimized.txt`
  - `avg_us=0.892`
  - `p99_us=0.912`
  - `throughput_mbps=1575.094`
- `compare-mmap.txt`
  - `avg_latency_gain_pct=99.65`
  - `p99_latency_gain_pct=99.85`
  - `throughput_gain_pct=24826.37`

## 通过口径说明

Day32 的“通过”重点不是某个绝对数值，而是：

1. baseline 与 optimized 的 workload 定义清楚
2. 前后结果可复读
3. 优化点和结果一一对应
4. 主链路在 guest 中完整结束，且无内核异常
