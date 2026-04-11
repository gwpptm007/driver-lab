# Day17 round compare 重新测试指南

## 1. 先清理旧的 LAST 指针（可选，但推荐）

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day17
rm -f records/LAST_baseline.txt records/LAST_round1.txt records/LAST_round2b.txt
```

## 2. 配好 perf 相关环境

```bash
export CROSS_COMPILE=aarch64-linux-gnu-
export PERF_SYSROOT=/usr/aarch64-linux-gnu
export PERF_LIB_DIRS=/usr/aarch64-linux-gnu/lib
```

## 3. 单轮重测

### baseline

```bash
./run_profile_collect.sh baseline
```

### round1

```bash
./run_profile_collect.sh round1
```

### round2b

```bash
./run_profile_collect.sh round2b
```

每轮完成后检查：

- `records/LAST_<profile>.txt`
- `records/<timestamp>-day17-<profile>-arm64-virt/metrics.env`
- `records/<timestamp>-day17-<profile>-arm64-virt/build_evidence/`

## 4. 整轮批量重测

```bash
./run_compare_rounds.sh
```

跑完后重点看：

- `records/compare-<timestamp>.csv`
- `records/compare-<timestamp>.md`
- `records/compare-<timestamp>-baseline_vs_round1.diff`
- `records/compare-<timestamp>-round1_vs_round2b.diff`
- `records/compare-<timestamp>-baseline_vs_round2b.diff`

## 5. 如何快速判断结果

### 三轮都 PASS，但 image/hash 都一样
优先判断为：fragment 没影响当前产物，或者配置没有真正生效。

### config sha 不一样，但 image sha 一样
说明 `.config` 变了，但当前 `virt + arm64` 的最终镜像没变；优先查看 diff 里改掉的 symbol 是否本来就不在当前产物路径中。

### image sha 也变了
这时再结合 `boot_ms / image_kib / rootfs_kib / memfree_kib` 判断裁剪收益。
