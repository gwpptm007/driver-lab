# day32 START_HERE

建议按下面顺序阅读和执行：

1. 先看 `README.md`，确认 day32 要收什么
2. 再看 `docs/01_plan.md`，理解 perf workload 和优化点
3. `source env/day32.env`
4. 跑 `make check && make run`
5. 查看 `records/<RUN_ID>/compare-mmap.txt` 与 `output/day32_perf_summary.md`
6. 若要补 host perf，再执行 `make perf-baseline` / `make perf-optimized`

## 当前包内 records 的结论

包内 `records/day32-local-001` 已经是一轮默认主链路通过的样例：

- `mmap-verify` 通过
- `compare-mmap` 已生成
- `bench-ioctl` 与 `bench-dma-lite` 已留证
- guest 完整结束

## 今天最重要的一句话

先固定 workload，再看热点；先做最小优化，再做前后对比。
