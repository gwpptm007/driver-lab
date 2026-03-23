# Day32 排障

## 1. QEMU 提前 timeout

先看：

- `records/<RUN_ID>/serial.log`
- `records/<RUN_ID>/qemu.stderr.log`
- `records/<RUN_ID>/run-summary.md`

Day32 默认 workload 较轻，一般不应触发 timeout。若触发，优先检查：

- `DAY32_PERF_ITER`
- `DAY32_DMA_LITE_ITER`
- 是否误把 `DAY32_PROFILE_MODE=full` 与额外 host perf 连续运行叠加了

## 2. baseline / optimized 结果差异不明显

优先检查：

- payload 是否太小
- iter/warmup 是否太少
- 是否真的在 baseline 路径里执行了 `GET_INFO + mmap + munmap`

## 3. host perf 缺失

若宿主没有 `perf`，默认 `make run` 仍可完成 day32 主链路。只有 `make perf-baseline` / `make perf-optimized` 依赖 `perf`。
