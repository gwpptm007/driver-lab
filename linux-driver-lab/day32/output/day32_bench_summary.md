# Day32 Bench Summary

## 默认主链路结论

- `mmap-verify`: 通过（`verify_ok=1`、`run_ok=1`、`irq_delta=2`）
- `bench-mmap-baseline`: 完整通过
- `bench-mmap-optimized`: 完整通过
- `compare-mmap`: 已生成前后对比
- `bench-ioctl`: 完整通过
- `bench-dma-lite`: 完整通过
- guest flow: 完整结束

## 关键数值

| workload | avg_us | p99_us | throughput_mbps |
|---|---:|---:|---:|
| mmap-baseline | 283.590 | 641.344 | 6.585 |
| mmap-optimized | 0.892 | 0.912 | 1575.094 |
| ioctl | 16.291 | 46.160 | 0.000 |
| dma-lite | 201041.057 | 207965.824 | 0.002 |

## Day32 一句话结论

把 `GET_INFO + mmap + munmap` 从热循环移出后，mmap bench 的平均延迟、尾延迟与吞吐都出现了显著改善；默认 records 已足以支撑 day32 验收通过。
