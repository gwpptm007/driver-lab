# W3 最终提交版报告（Day21 Submission）

## 一句话结论

W3 已经把 `arm64 + QEMU virt + BusyBox + demo_regmap.ko` 这条实验主线收敛成一套**可裁剪、可比较、可回归、可回滚**的最小内核实验平台；其中 **D15 → D16 的第一轮粗裁收益最稳，D18 代表当前最终形态，D20 提供继续闭环真实回归的自动化套件。**

## 1. 本轮最终交付了什么

- 一条固定口径的 baseline / trim1 / trim2 主线。
- 一份可复查的量化对比表，覆盖 `size / boot / mem / module 数`。
- 一套 smoke / trace / perf / stress 自动回归套件骨架。
- 一份可讲解、可回顾、可继续复用的最终总结报告。

## 2. 最稳的结论先讲清楚

### 2.1 D15 → D16：第一轮粗裁收益成立

- `Image`：38867 KiB → 37237 KiB，变化 -1630 KiB（-4.2%）。
- `boot_ms`：2008 ms → 2021 ms，变化 +13 ms，基本持平。
- `MemFree`：变化 +1152 KiB。
- `Slab`：变化 -144 KiB。
- `function_graph`：仍为 `yes`。

这说明在不破坏启动链路和关键观测能力的前提下，第一轮粗裁已经带来明确、稳定、可复述的收益。

### 2.2 D18：作为当前最终形态成立

D18 当前可以明确写成：

- `function_graph=yes`
- `perf=yes/yes`
- `pass_status=PASS`

但 D18 处在**新的 rootfs/perf 周期**，因此它更适合表达“当前最终形态成立”和“分类方法整理成立”，不适合和 D15 / D16 一起下无边界的纯收益排名结论。

## 3. 当前推荐配置

`arm64 + QEMU virt + BusyBox rootfs + demo_regmap.ko + debugfs/ftrace/function_graph/perf basic`

推荐理由：

- 路径已经跑通，且可持续复用。
- D15 → D20 的主线都围绕这套口径沉淀了脚本、records 和文档。
- 它兼顾了最小化目标与后续 trace/perf/回归需求。

## 4. Day20 当前状态应该怎么表述

Day20 当前不是“真实回归已经最终 PASS”，而是：

- `SUITE_READY=1`
- `DELIVERY_READY=1`
- `RUNTIME_READY=0`
- `REGRESSION_PASS=0`
- `LATEST_RECORD=20260315-121428-day20-all-arm64-virt`
- `LATEST_MODE=all`
- `LATEST_VERDICT=MISSING_INPUTS`
- `MISSING_ARTIFACTS=image,rootfs,dtb`

这表示 **回归套件本身已经成熟到可交付状态**，但这份代码包里真实运行件仍未闭环，所以最后一步真实 PASS 需要补齐 `Image/rootfs/dtb` 再跑。

## 5. 回滚与继续推进建议

### 回滚建议

- 以 D15 baseline `.config` 作为总回退点。
- 保留每轮 fragment、结果文档和 records，不把多轮改动揉成一次不可拆的变化。
- perf 异常时先区分内核配置问题与 rootfs 工具缺失，不要同时修改两边。

### 继续推进建议

- **要交付当前阶段成果**：优先使用本提交版报告 + Day19 对比表 + Day20 交付状态。
- **要拿到更硬的闭环**：补齐 `Image/rootfs/dtb` 后执行 Day20 真实回归。
- **要进一步提升报告可信度**：补齐 D15 / D16 的结构化 records，再回刷 Day19 / Day21。

## 6. 最终判断

> **Day21 当前已经达到“最终提交版总结报告”状态。**
>
> 它可以作为 W3 的最终压缩结论入口：既能讲清楚收益来自哪里，也能讲清楚哪些结论有边界、下一步该补什么。
