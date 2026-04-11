# 12_round_compare_flow - Day17 round1 / round2b 对比测试流程

## 1. 目标

把 Day17 的 baseline、round1、round2b 三个 profile 用同一套自动化链跑完，最终得到：

- 每轮各自的 `records/<timestamp>-<scenario>/baseline.csv`
- 一份跨轮次的 `compare-<timestamp>.csv`
- 一份便于阅读的 `compare-<timestamp>.md`

## 2. 前提条件

先保证 baseline 已经收口，尤其是：

- `perf_bin_ok=yes`
- `perf_smoke_ok=yes`
- `trace_smoke_ok=yes`
- `insmod_ok=yes`
- `snapshot_ok=yes`

并在宿主机设置好 perf 依赖路径：

```bash
export PERF_SYSROOT=/usr/aarch64-linux-gnu
export PERF_LIB_DIRS=/usr/aarch64-linux-gnu/lib
```

## 3. 最推荐的完整命令

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day17
./run_compare_rounds.sh
```

上面的脚本会自动做三件事：

1. 依次运行 `baseline -> round1 -> round2b`
2. 把每轮的最新 records 目录记到 `records/LAST_<profile>.txt`
3. 调用 `compare_results.py` 生成汇总结果

## 4. 如果想拆开运行

```bash
./run_profile_collect.sh baseline
./run_profile_collect.sh round1
./run_profile_collect.sh round2b
python3 ./compare_results.py
```

## 5. 最终看哪些文件

### 单轮结果

每轮都看自己的目录：

- `metrics.env`
- `baseline.csv`
- `serial.log`
- `perf_stat.txt`
- `snapshot.txt`

### 跨轮对比结果

最终看 `records/` 下最新的：

- `compare-<timestamp>.csv`
- `compare-<timestamp>.md`

## 6. 推荐读法

1. 先看 `compare.md` 的 status。
2. 再看 `boot_ms / image_kib / rootfs_kib` 的 delta。
3. 如果 round1 正常、round2b 异常，优先回看 `trim_round2b.fragment` 新增关闭项。
4. 如果三轮功能都正常，再考虑继续往 day18 做更细的量化和图表化。

## Round compare 证据链增强说明

当前版本在每轮 records 目录下都会额外保存 `build_evidence/`，并在批量对比后生成 `compare-*-*.diff`。  
如果你发现 baseline / round1 / round2b 的 boot_ms、image_kib 没差异，优先去看：

- `records/<...>/build_evidence/kernel.config`
- `records/<...>/build_evidence/artifact_evidence.env`
- `records/compare-*-baseline_vs_round1.diff`
- `records/compare-*-round1_vs_round2b.diff`

