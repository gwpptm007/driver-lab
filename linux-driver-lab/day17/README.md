# Day17 - 独立 W3 实验目录最终收口版

## 1. 原始需求与 Day17 的定位

Day17 的原始要求，不是继续在 day15/day16 上零散追加脚本，而是把前面已经验证过的几条链条统一收进 **day17 自己的目录**，形成一套能单独阅读、单独构建、单独测试、单独归档的实验闭环。

换句话说，Day17 要解决的核心问题是：

1. **把 day15 的 baseline 执行链搬进 day17**
   - arm64 `virt` 启动
   - BusyBox initramfs
   - `demo_regmap.ko`
   - `debugfs/tracing/function_graph`
   - guest / host 采样

2. **把 day16 的裁剪思路搬进 day17**
   - baseline / round1 / round2b profile
   - 结果归档
   - compare 汇总
   - diff 证据链

3. **把 perf 正式接入 day17**
   - 构建 arm64 perf
   - 注入 rootfs
   - 递归拷贝动态依赖
   - guest 内执行 `perf --version` / `perf stat`

因此，Day17 的最终定位是：

> **一个独立的 W3 实验工作台：它不仅能跑 baseline，还能做裁剪、做 perf、做证据链对比，并把结果保存在 day17 自己目录里。**

---

## 2. 为什么要这么做

前面 day15/day16 虽然已经积累了很多有效内容，但存在两个明显问题：

### 2.1 执行链分散
- day15 偏执行
- day16 偏裁剪分析
- 文档、脚本、结果目录分散在不同位置

这样会导致学习和复测成本较高：
- 不知道从哪里开始
- 不知道改 profile 后该跑哪个脚本
- 不知道 evidence 在哪里看

### 2.2 baseline 与 trim 链路没有统一收口
如果不把这些链条收进 day17，那么后续每次复测都要跨目录来回跳：
- 看 day15 构建链
- 看 day16 trim 片段
- 手工拼接结果

这不利于后续 day17 结束收口，也不利于后续继续扩展 day18/day19。

所以 Day17 做的事情，本质上是：

> **把“能跑”和“能分析”两件事合并成一套真正可复用的实验目录。**

---

## 3. Day17 最终完成了哪些内容

当前 Day17 已完成以下能力。

### 3.1 baseline 能力
- 独立 `apply_config.sh`
- 独立 `build.sh`
- 独立 `run_qemu.sh`
- 独立 `guest_collect.sh`
- 独立 `host_collect.sh`
- 独立 `records/` 结果归档

### 3.2 perf 集成能力
- `build_perf.sh` 单独构建 arm64 perf
- rootfs 自动注入 `/usr/bin/perf`
- 自动注入 `ld-linux-aarch64.so.1`、`libc.so.6`、`libm.so.6`
- `perf --version` 与 `perf stat -e task-clock -- /bin/true` 在 guest 内可通过

### 3.3 round compare 能力
- `run_profile_collect.sh`：单轮 profile 测试
- `run_compare_rounds.sh`：整轮 baseline / round1 / round2b 对比
- `compare_results.py`：自动生成 `compare-*.csv` / `compare-*.md` / `compare-*-*.diff`

### 3.4 证据链能力
每轮 records 下都保存：
- `build_evidence/kernel.config`
- `build_evidence/kernel.config.focus.txt`
- `build_evidence/applied_fragments.txt`
- `build_evidence/Image.sha256`
- `build_evidence/rootfs.img.sha256`
- `build_evidence/artifact_evidence.env`

这使得 Day17 不只是“跑完了”，而是：

> **你能追溯 profile 到底改了什么、最终 `.config` 是否变化、Image 是否变化、rootfs 是否变化。**

---

## 4. Day17 当前目录怎么理解

```text
 day17/
 ├── README.md
 ├── START_HERE.md
 ├── FIRST_RUN.md
 ├── notes_code_layout.md
 ├── apply_config.sh                  # profile 配置入口
 ├── build.sh                         # 构建 rootfs / perf / dtb / 启动 QEMU
 ├── build_perf.sh                    # 单独构建 arm64 perf
 ├── run_qemu.sh                      # 单独启动 QEMU
 ├── run_profile_collect.sh           # 单轮 profile 采样入口
 ├── run_compare_rounds.sh            # baseline/round1/round2b 批量跑
 ├── compare_results.py               # compare 汇总与 diff 生成
 ├── check_round_profiles.sh          # profile/evidence 一键排查
 ├── collect/
 │   ├── guest_collect.sh             # guest 侧采样
 │   ├── host_collect.sh              # host 侧串口采样与归档
 │   └── parse_meminfo.awk
 ├── config/
 │   ├── trace_baseline.fragment
 │   ├── trim_round1.fragment
 │   └── trim_round2b.fragment
 ├── docs/
 │   ├── 17_day17_final_summary_and_round_compare.md
 │   ├── 18_day17_final_test_process.md
 │   ├── 19_day17_implementation_walkthrough.md
 │   └── 20_day17_flowcharts_and_uml.md
 └── records/
```

### 4.1 最重要的四个脚本

#### `apply_config.sh`
负责 profile 选择与 fragment 叠加。它的职责不是直接“完成所有事情”，而是把 Day17 的三种 profile 明确表达出来：
- `baseline`
- `round1`
- `round2b`

#### `build.sh`
负责 Day17 的实际构建动作：
- 组 rootfs
- 注入 perf
- 打包 rootfs.img
- 注入 DT fragment
- 启动 QEMU

#### `collect/guest_collect.sh`
负责 guest 内的最小实验验证：
- tracing
- function_graph
- demo_regmap
- snapshot
- perf

#### `collect/host_collect.sh`
负责 host 侧串口自动化：
- 启动 QEMU
- 等 prompt
- 注入命令
- 从 `serial.log` 中提取 marker block
- 落盘 `metrics.env` / `baseline.csv`

---

## 5. 当前实现过程总结（按阶段理解）

### 阶段 A：先把 baseline 独立出来
Day17 最初先完成的是：
- baseline 构建链
- guest/host 采样链
- records 归档

这一步的目标是先证明：

> **不用再依赖 day15/day16，day17 自己也能跑通最小实验链。**

### 阶段 B：把 perf 接进 rootfs
这一阶段重点解决：
- perf 构建
- 动态加载器路径
- libc/libm 依赖
- guest 内 `which perf` 能看到，但执行 `not found` 的问题
- `/bin/true` 缺失导致 `perf stat` workload 失败的问题

这一阶段结束后，Day17 实现了：
- `perf --version` 正常
- `perf stat -e task-clock -- /bin/true` 正常

### 阶段 C：把 round compare 证据链做起来
这一阶段重点不是功能，而是回答：

> round1 / round2b 真的改到最终 `.config` 和 `Image` 了吗？

因此增加了：
- `build_evidence/`
- `kernel.config`
- `artifact_evidence.env`
- `compare-*.diff`
- `check_round_profiles.sh`

### 阶段 D：修正无效裁剪项，形成真正有收益的 round1/round2b
前几版虽然 profile 分支链通了，但裁剪项很多在 baseline 中本来就是 `n`，所以 `.config` 没变化。

最终 fix5 改成直接裁当前 baseline 中真实为 `y` 的顶层项：
- round1：`PCI` + `SCSI`
- round2b：在 round1 上继续去掉 `NET`

这一阶段完成后，round compare 才真正开始有意义。

---

## 6. 当前验收什么

Day17 当前的验收分三层。

### 6.1 baseline 验收
要求至少满足：
- `boot_ok=yes`
- `debugfs_ok=yes`
- `tracing_ok=yes`
- `function_graph_ok=yes`
- `trace_smoke_ok=yes`
- `insmod_ok=yes`
- `snapshot_ok=yes`
- `trigger_ok=yes`
- `dmesg_warn=no`

### 6.2 perf 验收
要求至少满足：
- `perf_bin_ok=yes`
- `perf_smoke_ok=yes`

### 6.3 round compare 验收
要求至少满足：
- baseline / round1 / round2b 三轮都 `PASS`
- `compare-*.md` 正常生成
- `compare-*-*.diff` 正常生成
- `kernel_config_sha256` / `kernel_image_sha256` 能反映真实 profile 差异

---

## 7. 当前真实测试结论（基于 records/ 最新记录）

当前 `records/compare-20260314-231137.md` 的最终结论是：

### baseline
- status = PASS
- boot_ms = 2019
- image_kib = 34621
- rootfs_kib = 8128
- memfree_kib = 951424
- perf = yes / yes

### round1
- status = PASS
- boot_ms = 3028
- image_kib = 33539
- rootfs_kib = 8128
- memfree_kib = 952800
- perf = yes / yes

### round2b
- status = PASS
- boot_ms = 2018
- image_kib = 27417
- rootfs_kib = 8128
- memfree_kib = 962060
- perf = yes / yes

也就是说：

- round1 相比 baseline：
  - 镜像减少 **1082 KiB**
  - 空闲内存增加 **1376 KiB**
- round2b 相比 baseline：
  - 镜像减少 **7204 KiB**
  - 空闲内存增加 **10636 KiB**

而且三轮功能回归都通过。

这说明：

> **Day17 的 round compare 已经从“框架阶段”进入“结果可用阶段”。**

---

## 8. round1 / round2b 该怎么理解

### 8.1 round1
定位为：

> **保守裁剪版**

它主要去掉：
- PCI 整条链
- SCSI 相关链

收益中等，风险相对低，更适合作为后续默认裁剪候选。

### 8.2 round2b
定位为：

> **极限最小实验版**

它在 round1 基础上继续去掉网络栈。

收益很明显，但要强调边界：
- 对当前 `demo_regmap + tracing + perf + serial` 链条是成立的
- 对后续所有网络相关实验并不一定适合

因此 round2b 不建议直接替代通用 baseline，而更适合作为：

> **面向当前最小驱动/tracing/perf 实验场景的极限最小配置。**

---

## 9. 推荐阅读顺序

如果你是第一次看 Day17，推荐顺序如下：

1. `README.md`  
   先搞清楚 Day17 为什么存在、当前完成了什么。

2. `docs/17_day17_final_summary_and_round_compare.md`  
   看最终结论和 round1 / round2b 的收益怎么定。

3. `docs/18_day17_final_test_process.md`  
   看完整测试过程应该怎么一步步跑。

4. `docs/19_day17_implementation_walkthrough.md`  
   看代码链、脚本链、结果链到底怎么串起来。

5. `docs/20_day17_flowcharts_and_uml.md`  
   看流程图、时序图、UML，帮助快速建立整体结构感。

6. `records/compare-20260314-231137.md`  
   看这次最终真实跑出来的数据。

---

## 10. 一句话总结

> **Day17 最终完成了独立 baseline、perf 集成、round compare、evidence 证据链四条主线的统一收口；当前不仅能独立跑通实验，还能用 round1 / round2b 做真实、可追溯、可解释的裁剪对比。**
