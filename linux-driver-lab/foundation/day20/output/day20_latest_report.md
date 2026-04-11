# Day20 最新结果

- record_dir: 20260315-121428-day20-all-arm64-virt
- mode: all
- verdict: MISSING_INPUTS

## 快速判断

最近一次是 dry-run，但运行件还没齐。先补输入件，再跑真实回归。

- missing_artifacts: image,rootfs,dtb

## 关键状态

- DEBUGFS_OK: (n/a)
- DEMO_INSMOD_OK: (n/a)
- SNAPSHOT_OK: (n/a)
- TRIGGER_OK: (n/a)
- RMMOD_OK: (n/a)
- TRACING_OK: (n/a)
- FGRAPH_OK: (n/a)
- PERF_OK: (n/a)
- STRESS_OK: (n/a)
- DMESG_CLEAN: (n/a)

## 下一步建议

1. 打开 `records/<record_dir>/summary.txt` 看总判断。
2. 打开 `records/<record_dir>/host_runner.log` 看宿主机侧阶段。
3. 打开 `records/<record_dir>/serial.log` 看 guest 侧原始输出。
4. 必要时再看 trace/perf/snapshot/dmesg 原始文本。
