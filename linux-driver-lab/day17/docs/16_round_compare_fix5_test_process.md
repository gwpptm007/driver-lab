# Day17 round compare（fix5）详细测试过程

这份流程专门对应 fix5：
- round1 关闭 `CONFIG_PCI` / `CONFIG_SCSI`
- round2b 在 round1 基础上再关闭 `CONFIG_NET`

目标不是立刻追求“收益最大”，而是先验证 **profile 差异一定能进入最终 `.config` 与 `Image`**。

## 1. 环境准备

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day17
export CROSS_COMPILE=aarch64-linux-gnu-
export PERF_SYSROOT=/usr/aarch64-linux-gnu
export PERF_LIB_DIRS=/usr/aarch64-linux-gnu/lib
```

## 2. 建议先清理旧的 LAST 指针

```bash
rm -f records/LAST_baseline.txt records/LAST_round1.txt records/LAST_round2b.txt
```

## 3. 批量跑 baseline / round1 / round2b

```bash
./run_compare_rounds.sh
```

预期：
- 三轮都生成新的 records 目录；
- records 顶层生成新的 `compare-*.md/csv/diff`；
- 若 round1/round2b 生效，diff 文件不应再是 `# no diff or source file missing`。

## 4. 先看汇总结果

```bash
cd records
ls -lt compare-* | head
cat "$(ls -t compare-*.md | head -n 1)"
```

重点看：
- `kernel_config_sha256`
- `kernel_image_sha256`
- `config_diff_vs_baseline`

### 4.1 正常命中 fix5 的信号

至少应该出现下面之一：
- baseline / round1 的 `kernel_config_sha256` 不同；
- round1 / round2b 的 `kernel_config_sha256` 不同；
- `compare-*-baseline_vs_round1.diff` 不再是空 diff；
- `kernel_image_sha256` 随 profile 变化。

## 5. 再看 diff 文件

```bash
sed -n '1,200p' "$(ls -t compare-*-baseline_vs_round1.diff | head -n 1)"
sed -n '1,200p' "$(ls -t compare-*-round1_vs_round2b.diff | head -n 1)"
```

预期：
- `baseline_vs_round1.diff` 至少能看到 `CONFIG_PCI` / `CONFIG_SCSI` 的变化；
- `round1_vs_round2b.diff` 至少能看到 `CONFIG_NET` 的变化。

## 6. 如果还怀疑 profile 没生效，直接跑排查脚本

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day17
chmod +x check_round_profiles.sh
./check_round_profiles.sh
```

报告会落到：

```text
records/profile_check_<timestamp>.log
```

重点看：
- `applied_fragments.txt`
- `kernel.config sha256`
- `diff baseline vs round1`
- `diff round1 vs round2b`

## 7. 最终验收标准

### 功能侧
- `status=PASS`
- `perf_bin_ok=yes`
- `perf_smoke_ok=yes`
- `trace_smoke_ok=yes`
- `insmod_ok=yes`

### 证据链侧
- round1 的 `.config` 相对 baseline 有变化；
- round2b 的 `.config` 相对 round1 有变化；
- 至少有一轮 `kernel_image_sha256` 发生变化。

## 8. 结果解读建议

### 情况 A：config 变了，image 也变了
说明 fix5 达成目标，profile 差异已经进入最终产物。

### 情况 B：config 变了，但 image 没变
说明裁掉的内容不在当前最终镜像路径里，后续要换更贴近当前 baseline 的候选项。

### 情况 C：config 仍不变
优先回看：
- `build_evidence/applied_fragments.txt`
- `compare-*.diff`
- `check_round_profiles.sh` 输出
