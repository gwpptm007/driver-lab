# Day17 最终结果总结 + round1 / round2b 对比结论

## 1. 文档目的

本文对 Day17 的最终实现与测试结果做统一收口，重点回答以下问题：

1. Day17 最终完成了什么
2. Day17 的 baseline 是否已经独立稳定
3. round1 / round2b 是否真正作用到了最终 `.config` 与内核镜像
4. 两轮裁剪各自带来了什么收益
5. round1 / round2b 应该如何定位和使用

---

## 2. Day17 最终完成内容

Day17 最终已经完成一套**独立闭环实验链**，不再依赖 day15/day16 才能完成运行与验证，主要包括：

- 独立的 `apply_config.sh`
- 独立的 `build.sh`
- 独立的 `run_qemu.sh`
- 独立的 `guest_collect.sh`
- 独立的 `host_collect.sh`
- 独立的 `run_profile_collect.sh`
- 独立的 `run_compare_rounds.sh`
- 独立的 compare 汇总与 diff 证据链
- 独立的 perf 集成与验证流程

Day17 最终目标已经从“继续依附 day15/day16 的实验目录”，演进为：

> **一个可单独构建、单独启动、单独采样、单独对比、单独归档的 W3 实验工作台。**

---

## 3. Day17 baseline 最终状态

Day17 baseline 已完成以下能力验证：

- QEMU arm64 `virt` 正常启动
- BusyBox initramfs 正常工作
- `demo_regmap.ko` 可正常加载
- `debugfs` 正常
- `tracing` 正常
- `function_graph` 正常
- `trace smoke` 正常
- `perf` 已成功集成到 guest
- `perf --version` 正常
- `perf stat` 基础 smoke 正常
- `guest_collect.sh` 正常输出
- `host_collect.sh` 正常采集并归档
- `baseline.csv` / `metrics.env` / `serial.log` 可稳定生成

这意味着 Day17 的 baseline 已经不是“概念验证”，而是可以作为后续裁剪、回归和扩展的稳定基线。

---

## 4. round compare 过程中的关键问题与修复过程

### 4.1 初始问题：profile 传入了，但最终 `.config` 没变化

在 round compare 的早期版本中，虽然 `baseline / round1 / round2b` 三轮都能跑通，且 `applied_fragments.txt` 也能记录不同 fragment，但最终 evidence 显示：

- `kernel_config_sha256` 三轮完全一致
- `kernel_image_sha256` 三轮完全一致
- `compare diff` 为 `NO DIFF`

这说明当时的问题不是 profile 没传入，而是：

> **fragment 虽然参与了流程，但对当前 baseline 的最终配置没有产生净变化。**

进一步检查发现，当时很多想裁掉的项，在 baseline 中本来就已经是 `n`，因此即使 fragment 被应用，也不会改变 `.config`。这个阶段的 profile 分支链是通的，但裁剪项选择无效。

### 4.2 fix5：重新选择“当前 baseline 中真实为 y 的顶层项”

为了解决“fragment 应用但不生效”的问题，后续版本不再继续裁“可能已经是 n 的 SoC 子项”，而改成直接选择当前 baseline 中明确为 `y` 的顶层开关：

- round1：关闭 `PCI`、`SCSI`
- round2b：在 round1 基础上进一步关闭 `NET`

这样做的目的不是一开始就追求最优裁剪，而是：

> **先确认 round1 / round2b 真的能把差异打进最终 `.config` 和 `Image`。**

最终结果证明这一策略成功。

---

## 5. 本轮最终对比结果

本轮最终对比结果如下：

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

从 evidence 看，三轮已经出现明确差异：

- `kernel_config_sha256` 三轮不同
- `kernel_image_sha256` 三轮不同
- `compare diff` 不再是空结果
- `baseline_vs_round1.diff` 约 3.7 万字节
- `round1_vs_round2b.diff` 约 4.1 万字节
- `baseline_vs_round2b.diff` 约 7.1 万字节

这说明：

> **Day17 的 round compare 配置差异链已经完全打通。**

---

## 6. round1 结论

### 6.1 round1 裁剪内容

从 `baseline_vs_round1.diff` 可见，round1 主要裁掉了 PCI/SCSI 相关链路，包括但不限于：

- `CONFIG_PCI`
- `CONFIG_PCIEPORTBUS`
- `CONFIG_PCI_MSI`
- 一系列 PCI controller / host / endpoint 相关项
- `CONFIG_BLK_MQ_PCI`
- `CONFIG_ACPI_MCFG`
- `CONFIG_SCSI`

这说明 round1 的核心定位是：

> **在不影响当前最小实验主链的前提下，去掉与 virt+serial+demo_regmap+tracing/perf 无关的 PCI/SCSI 相关支持。**

### 6.2 round1 收益

与 baseline 相比：

- `image_kib` 从 34621 降到 33539  
  **减少 1082 KiB**
- `memfree_kib` 从 951424 升到 952800  
  **增加 1376 KiB**

这说明 round1 已经产生了真实裁剪收益。

### 6.3 round1 风险与注意点

当前 round1 的 `boot_ms=3028`，相对 baseline 增加约 1009 ms。  
但这个值不宜立刻直接定性为“裁剪导致启动变慢”，更合理的解释包括：

- 单次启动测量抖动
- QEMU 串口 prompt 出现时机波动
- host_collect 对首个 prompt 的捕捉时刻差异

因此：

> **round1 的启动时间结论需要至少再做 2~3 次重复测试后，再取中位数或平均值。**

---

## 7. round2b 结论

### 7.1 round2b 裁剪内容

从 `round1_vs_round2b.diff` 可见，round2b 在 round1 的基础上，进一步去掉了网络栈整条链路，包括但不限于：

- `CONFIG_NET`
- `CONFIG_PACKET`
- `CONFIG_UNIX`
- `CONFIG_INET`
- `CONFIG_IPV6`
- `CONFIG_NETFILTER`
- `CONFIG_NF_CONNTRACK`
- `CONFIG_NF_NAT`
- `CONFIG_NETFILTER_XTABLES`
- 以及大量网络子项

这说明 round2b 的核心定位是：

> **构造一个不依赖内核网络栈的极限最小实验版内核。**

### 7.2 round2b 收益

与 baseline 相比：

- `image_kib` 从 34621 降到 27417  
  **减少 7204 KiB**
- `memfree_kib` 从 951424 升到 962060  
  **增加 10636 KiB**
- `boot_ms` 为 2018，与 baseline 的 2019 基本一致

这是一组非常明显且有工程意义的收益。

### 7.3 round2b 边界

虽然 round2b 当前所有验收项仍然 PASS，但它的成立是有边界的。

当前 day17 验收链依赖的是：

- serial 控制台
- BusyBox rootfs
- `demo_regmap.ko`
- `debugfs`
- `tracing`
- `function_graph`
- `perf`

这些都不要求内核网络栈必须启用，因此 round2b 可以正常通过。

但如果后续实验涉及：

- socket / TCP / UDP
- network namespace
- ip / ping / route
- netfilter
- virtio-net
- 网络调试、抓包、连通性验证

那么 round2b 就不适合作为通用 baseline。

因此 round2b 更适合定义为：

> **面向当前最小驱动/tracing/perf 实验场景的极限最小化配置。**

而不适合作为后续所有实验的通用内核配置。

---

## 8. baseline / round1 / round2b 的工程定位

### baseline

定位为：

> **功能完整、实验稳定的标准基线**

适合：
- 新功能接入
- 回归验证
- 问题排查
- 作为后续所有 profile 的对照组

### round1

定位为：

> **保守裁剪版**

特点：
- 去掉明显无关的 PCI/SCSI 链
- 收益中等
- 风险相对低
- 更适合作为“通用裁剪候选版”

### round2b

定位为：

> **极限最小实验版**

特点：
- 进一步去掉网络栈
- 镜像缩减非常明显
- 空闲内存收益明显
- 当前 day17 验收通过
- 但适用边界更窄，不适合作为通用开发基线

---

## 9. 最终结论

Day17 最终已经完成两件关键工作：

### 9.1 完成独立 baseline 收口

Day17 已经具备：

- 独立构建
- 独立启动
- 独立采样
- 独立 perf 集成
- 独立结果归档
- 独立对比分析

它已经是一套完整、稳定、可复用的实验链。

### 9.2 完成 round compare 有效验证

经过多轮修正后，round1 / round2b 已经不再停留在“profile 形式存在”，而是真正作用到了：

- 最终 `.config`
- 最终 `Image`
- 最终对比结果

其中：

- round1 已实现中等幅度裁剪
- round2b 已实现大幅度裁剪
- 两轮在当前 day17 验收项下均未出现功能回归

因此可以明确写出：

> **Day17 的 round compare 已从“框架搭建阶段”进入“结果可用阶段”。**

---

## 10. 建议的后续工作

### 建议 1：对 round1 启动时间做重复性验证

建议分别对：

- baseline
- round1
- round2b

各再重复测试 2~3 轮，并对 `boot_ms` 取中位数或平均值，再写最终性能结论。

### 建议 2：将 round1 作为默认“通用裁剪版”候选

因为它收益明确、风险较低，更适合作为后续默认精简版配置继续演进。

### 建议 3：将 round2b 单独标注为“极限最小实验版”

避免后续误用到需要网络功能的实验场景。

### 建议 4：把本轮 compare 结果固定到 docs

建议保留以下证据：

- `compare-*.md`
- `compare-*.csv`
- `compare-*-baseline_vs_round1.diff`
- `compare-*-round1_vs_round2b.diff`
- 三轮各自的 `build_evidence/`

作为 Day17 的正式收口材料。

---

## 11. 一句话总结

> **Day17 最终完成了独立 baseline 构建与 perf/tracing 实验闭环，并成功验证了 round1 与 round2b 两级裁剪方案；其中 round1 适合做保守通用裁剪，round2b 适合做极限最小实验版，两者在当前 day17 验收项下均保持功能通过。**
