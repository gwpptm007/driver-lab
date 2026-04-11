# 13_round_compare_reading_guide - Day17 对比结果怎么读

## 1. status 的含义

`compare_results.py` 会给每个 profile 生成一个 `pass_status`：

- `PASS`：核心功能全部正常，perf 也通过
- `WARN`：核心功能正常，但 perf 或 dmesg 有提醒项
- `FAIL`：核心功能有回归

## 2. 关键数字怎么看

### boot_ms

越小越好，但不能为了降启动时间牺牲 tracing / perf / demo 模块可用性。

### image_kib

表示内核镜像大小。round1 / round2b 的直接收益之一就是它下降。

### rootfs_kib

如果 rootfs 基本不变，说明这轮收益主要来自内核裁剪，不是 rootfs 变化带来的假收益。

### memfree_kib

通常越大越好，但它本身会有轻微抖动，所以要结合 `slab_kib` 一起看。

## 3. 功能项优先级

观察顺序建议如下：

1. `boot_ok`
2. `debugfs_ok / tracing_ok / function_graph_ok`
3. `trace_smoke_ok`
4. `perf_bin_ok / perf_smoke_ok`
5. `insmod_ok / snapshot_ok / trigger_ok`
6. `dmesg_warn / remarks`

## 4. 常见判断

### 情况 A：round1 全绿，round2b FAIL

优先怀疑 round2b 新增关闭项裁得过深。

### 情况 B：功能正常，但 boot_ms 没下降

说明这轮裁剪收益不明显，后续可以重新评估 fragment 是否值得保留。

### 情况 C：image_kib 下降明显，但 perf 变成 no

这类通常不是“收益”，而是把 perf 相关链路裁掉了，属于功能回归。

## Round compare 证据链增强说明

当前版本在每轮 records 目录下都会额外保存 `build_evidence/`，并在批量对比后生成 `compare-*-*.diff`。  
如果你发现 baseline / round1 / round2b 的 boot_ms、image_kib 没差异，优先去看：

- `records/<...>/build_evidence/kernel.config`
- `records/<...>/build_evidence/artifact_evidence.env`
- `records/compare-*-baseline_vs_round1.diff`
- `records/compare-*-round1_vs_round2b.diff`

