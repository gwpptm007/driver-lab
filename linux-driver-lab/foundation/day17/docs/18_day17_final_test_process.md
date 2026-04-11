# Day17 最终详细测试过程

## 1. 文档目的

本文整理 Day17 最终版从环境准备、baseline 验证、perf 集成验证，到 round1 / round2b 对比测试的完整测试过程。  
目标是做到：

- 可以按本文一步步复现 Day17 最终结果
- 可以明确每一步要执行什么命令
- 可以明确每一步应该看什么结果
- 可以把最终 evidence 与 compare 结果归档下来

---

## 2. 测试前准备

### 2.1 进入 Day17 目录

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day17
```

### 2.2 准备交叉编译与 perf 依赖路径

```bash
export CROSS_COMPILE=aarch64-linux-gnu-
export PERF_SYSROOT=/usr/aarch64-linux-gnu
export PERF_LIB_DIRS=/usr/aarch64-linux-gnu/lib
```

### 2.3 建议检查工具可用性

```bash
aarch64-linux-gnu-gcc --version
qemu-system-aarch64 --version
python3 --version
```

---

## 3. baseline 单轮测试过程

### 3.1 应用 baseline profile

```bash
PROFILE=baseline ./apply_config.sh
```

### 3.2 构建并启动

```bash
PERF_REQUIRED=yes PERF_MODE=auto ./build.sh
```

正常情况下会自动启动 QEMU，并进入 guest。

---

## 4. guest 内手工验证过程

进入 guest 后，依次执行以下命令。

### 4.1 模块加载

```sh
insmod /demo_regmap.ko
```

### 4.2 perf 基础验证

```sh
which perf
perf --version
perf stat -e task-clock -- /bin/true
```

### 4.3 采样脚本执行

```sh
/bin/day17_guest_collect.sh
cat /tmp/day17-baseline/metrics.env
```

### 4.4 期望关键字段

`metrics.env` 中至少应看到：

```text
boot_ok=yes
debugfs_ok=yes
tracing_ok=yes
function_graph_ok=yes
trace_smoke_ok=yes
perf_bin_ok=yes
perf_smoke_ok=yes
insmod_ok=yes
snapshot_ok=yes
trigger_ok=yes
dmesg_warn=no
```

这说明 Day17 baseline 的最小实验主链已经通过。

---

## 5. host 自动采样过程

在宿主机执行：

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day17/collect
SCENARIO_ID=day17-baseline-arm64-virt ./host_collect.sh
```

正常情况下会在 `day17/records/` 下生成一条新的记录目录。

### 5.1 需要检查的文件

进入对应 records 目录，重点检查：

```bash
ll
cat metrics.env
cat baseline.csv
```

应至少生成：

- `host_metrics.env`
- `guest_metrics.env`
- `metrics.env`
- `baseline.csv`
- `serial.log`
- `snapshot.txt`
- `perf_version.txt`
- `perf_stat.txt`

---

## 6. perf 集成验证过程

Day17 最终版已经支持 perf 自动集成。  
验证重点如下。

### 6.1 宿主机检查 perf 产物

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day17
ls -l output/perf/perf
sed -n '1,200p' output/perf/perf_bundle_manifest.txt
```

### 6.2 宿主机检查 rootfs 中动态库

```bash
find rootfs -name 'ld-linux-aarch64.so*' -o -name 'libc.so*' -o -name 'libm.so*'
```

预期至少包括：

```text
rootfs/lib/ld-linux-aarch64.so.1
rootfs/lib/libc.so.6
rootfs/lib/libm.so.6
```

### 6.3 guest 中检查 perf

```sh
which perf
perf --version
cat /etc/day17_perf_manifest.txt
```

### 6.4 smoke 验证

```sh
perf stat -e task-clock -- /bin/true
```

若成功，则说明：

- perf 二进制可执行
- 动态加载器正确
- libc / libm 依赖正确
- perf runtime 正常

---

## 7. round compare 测试入口

Day17 最终版支持两种测试方式：

### 7.1 单 profile 一键跑

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day17
./run_profile_collect.sh baseline
./run_profile_collect.sh round1
./run_profile_collect.sh round2b
```

### 7.2 整轮一键跑并自动汇总

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day17
./run_compare_rounds.sh
```

推荐最终收口时使用 **整轮一键跑**。

---

## 8. round compare 最终测试过程（推荐）

### 8.1 清理旧指针（可选）

```bash
rm -f records/LAST_baseline.txt records/LAST_round1.txt records/LAST_round2b.txt
```

### 8.2 直接整轮执行

```bash
./run_compare_rounds.sh
```

这一步会自动完成：

1. baseline 构建、启动、采样
2. round1 构建、启动、采样
3. round2b 构建、启动、采样
4. evidence 保存
5. compare 汇总
6. diff 生成

---

## 9. round compare 跑完后怎么看

### 9.1 顶层 compare 文件

```bash
cd records
ls -lt compare-* | head
cat "$(ls -t compare-*.md | head -n 1)"
```

应看到：

- `compare-<timestamp>.csv`
- `compare-<timestamp>.md`
- `compare-<timestamp>-baseline_vs_round1.diff`
- `compare-<timestamp>-round1_vs_round2b.diff`
- `compare-<timestamp>-baseline_vs_round2b.diff`

### 9.2 evidence 文件

查看每轮 evidence：

```bash
cat "$(cat LAST_baseline.txt)"/build_evidence/artifact_evidence.env
cat "$(cat LAST_round1.txt)"/build_evidence/artifact_evidence.env
cat "$(cat LAST_round2b.txt)"/build_evidence/artifact_evidence.env
```

重点字段：

- `kernel_config_sha256`
- `kernel_image_sha256`
- `rootfs_img_sha256`
- `kernel_image_bytes`
- `rootfs_img_bytes`

### 9.3 diff 文件

```bash
sed -n '1,200p' "$(ls -t compare-*-baseline_vs_round1.diff | head -n 1)"
sed -n '1,200p' "$(ls -t compare-*-round1_vs_round2b.diff | head -n 1)"
```

这一步用于确认：

- round1 是否真的作用到了 `.config`
- round2b 是否进一步作用到了 `.config`

---

## 10. 最终一轮测试的结果解读

### 10.1 baseline

- `boot_ms = 2019`
- `image_kib = 34621`
- `memfree_kib = 951424`

### 10.2 round1

- `boot_ms = 3028`
- `image_kib = 33539`
- `memfree_kib = 952800`

相对 baseline：

- 镜像减少 **1082 KiB**
- 空闲内存增加 **1376 KiB**

同时 diff 显示 round1 主要裁掉了：

- `CONFIG_PCI`
- `CONFIG_PCIE...`
- `CONFIG_SCSI`
- 一批 PCI controller / host / endpoint 相关项

### 10.3 round2b

- `boot_ms = 2018`
- `image_kib = 27417`
- `memfree_kib = 962060`

相对 baseline：

- 镜像减少 **7204 KiB**
- 空闲内存增加 **10636 KiB**

同时 diff 显示 round2b 在 round1 基础上进一步裁掉了：

- `CONFIG_NET`
- `CONFIG_PACKET`
- `CONFIG_UNIX`
- `CONFIG_INET`
- `CONFIG_IPV6`
- `CONFIG_NETFILTER`
- `CONFIG_NF_CONNTRACK`
- `CONFIG_NF_NAT`
- `CONFIG_NETFILTER_XTABLES`

---

## 11. 如何判断测试是否成功

### baseline 成功标准

- `perf_bin_ok=yes`
- `perf_smoke_ok=yes`
- `tracing_ok=yes`
- `function_graph_ok=yes`
- `insmod_ok=yes`
- `snapshot_ok=yes`

### round compare 成功标准

- 三轮 `status=PASS`
- `kernel_config_sha256` 不再三轮相同
- `kernel_image_sha256` 不再三轮相同
- `baseline_vs_round1.diff` 不再是空 diff
- `round1_vs_round2b.diff` 不再是空 diff

### 当前最终结果

Day17 最终版已经满足以上条件。

---

## 12. 推荐的最终归档材料

建议最终保留以下文件作为 Day17 收口材料：

1. `records/compare-*.md`
2. `records/compare-*.csv`
3. `records/compare-*-baseline_vs_round1.diff`
4. `records/compare-*-round1_vs_round2b.diff`
5. 每轮 `records/<...>/build_evidence/`
6. 本文档
7. `17_day17_final_summary_and_round_compare.md`

---

## 13. 后续建议

### 建议 1：重复测试 boot_ms

建议对：

- baseline
- round1
- round2b

各重复 2~3 次，取中位数或平均值，避免单次启动抖动带来的误判。

### 建议 2：将 round1 作为默认通用裁剪候选

因为它收益明确、风险较低，适合作为后续默认精简版继续演进。

### 建议 3：将 round2b 仅用于极限最小实验场景

因为它已经移除了内核网络栈，不适合后续需要网络相关实验的通用场景。

---

## 14. 一句话总结

> **Day17 最终版已经完成 baseline、perf、round1、round2b 的完整测试闭环；其中 round1 提供保守裁剪收益，round2b 提供极限最小化收益，两者在当前 day17 验收项下均保持功能通过。**
