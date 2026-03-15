# W3 一页总结（Day21 One-Pager）

## 背景

W3 的目标是在保留 `debugfs + ftrace + function_graph + perf basic` 的前提下，把 `arm64 + QEMU virt + BusyBox + demo_regmap.ko` 收敛成最小、可比较、可回归的实验平台。

## 方法

- D15：baseline 冻结
- D16：第一轮粗裁
- D18：第二轮分类裁剪与 perf 进入当前最终阶段
- D19：量化对比
- D20：自动回归套件

## 核心数据

| 阶段 | Image | boot | MemFree | perf |
|---|---:|---:|---:|---|
| D15 | 38867 KiB | 2008 ms | 968564 KiB | no |
| D16 | 37237 KiB | 2021 ms | 969716 KiB | kernel-side-kept,userland-not-shown |
| D18 | 27417 KiB | 2054 ms | 961808 KiB | yes/yes |

**最稳结论：** D15 → D16 的 `Image` 缩小 -1630 KiB（-4.2%），boot 基本持平。

## 当前推荐配置

`arm64 + QEMU virt + BusyBox rootfs + demo_regmap.ko + debugfs/ftrace/function_graph/perf basic`

## Day20 状态

- SUITE_READY=1
- DELIVERY_READY=1
- RUNTIME_READY=0
- REGRESSION_PASS=0
- latest_verdict=MISSING_INPUTS
- missing_artifacts=image,rootfs,dtb

## 回滚方案

- 回退到 D15 baseline `.config` + rootfs + QEMU 启动参数
- 每轮裁剪保留单独 records / 结果文档
- perf 异常时不要同时修改内核和 rootfs
