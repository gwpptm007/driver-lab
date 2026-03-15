# W3 最终总结报告（Day21 正式版）

## 1. 背景

W3 的目标不是继续堆新驱动功能，而是把已经跑通的 `arm64 + QEMU virt + BusyBox + demo_regmap.ko` 实验环境，收敛成一套**可裁剪、可比较、可回归、可回滚**的最小内核实验平台。围绕这个目标，D15-D20 依次完成了 baseline 冻结、第一轮粗裁、第二轮分类裁剪、量化对比和自动回归套件收口。

## 2. 方法

- **D15**：固定 baseline 配置、启动参数、rootfs 路线和验证对象。
- **D16**：执行第一轮粗裁，先去掉明显无关驱动与子系统，同时保留 debugfs / ftrace / function_graph 等观测能力。
- **D17 / D18**：完成 rootfs 工具链补齐与第二轮分类裁剪，把 `perf` 工具带入当前最终阶段。
- **D19**：整理 baseline / trim1 / trim2 的 `size / boot / mem / module 数` 对比，并补上风险矩阵。
- **D20**：建立 smoke / trace / perf / stress 四类自动回归入口，以及 latest / summary / verify / suite 交付接口。

## 3. 核心数据

| 阶段 | Image | rootfs | boot | MemFree | Slab | 运行时模块数 | function_graph | perf | 说明 |
|---|---:|---:|---:|---:|---:|---:|---|---|---|
| D15 baseline | 38867 KiB | 1181 KiB | 2008 ms | 968564 KiB | 12252 KiB | 1 | yes | no | baseline 起点 |
| D16 trim1 | 37237 KiB | 1181 KiB | 2021 ms | 969716 KiB | 12108 KiB | 1 | yes | kernel-side-kept,userland-not-shown | 第一轮粗裁 |
| D18 trim2 | 27417 KiB | 8128 KiB | 2054 ms | 961808 KiB | 8236 KiB | 1 | yes | yes/yes | 第二轮分类裁剪 |

**最稳的量化结论来自 D15 → D16：**

- `Image` 从 38867 KiB 降到 37237 KiB，变化 -1630 KiB（-4.2%）。
- `boot_ms` 从 2008 ms 到 2021 ms，变化 +13 ms，可视为基本持平。
- `MemFree` 变化 +1152 KiB，`Slab` 变化 -144 KiB，说明第一轮粗裁在不破坏启动链路的前提下带来了轻微正向收益。

**D18 需要带边界说明：** D18 已进入带 `perf` 的新 rootfs 周期，`function_graph=yes`、`perf=yes/yes`、`pass_status=PASS` 可以直接写成“当前最终形态成立”，但其 `rootfs` 与 `boot` 数字不能不加说明地和 D15 / D16 做纯收益排名。

## 4. 结论

1. **W3 已经形成完整收口链路。** 目前仓库里已经具备 baseline → trim1 → trim2 → compare → regression 的工程化闭环。
2. **第一轮粗裁收益已经有明确量化证据。** D15 到 D16 的镜像缩减最稳，且 boot 基本持平，说明“先去掉明显无关项”的策略是正确的。
3. **当前推荐配置已经明确。** 推荐继续以 `arm64 + QEMU virt + BusyBox rootfs + demo_regmap.ko + debugfs/ftrace/function_graph/perf basic` 作为后续主线口径。
4. **第二轮分类裁剪更适合作为“当前最终形态 + 方法表达整理”。** D18 证明了分类表达成立，并把 `perf` 用户态带入当前最终阶段。
5. **Day20 已达到交付级回归套件骨架。** 当前状态为 `SUITE_READY=1`、`DELIVERY_READY=1`、`RUNTIME_READY=0`、`REGRESSION_PASS=0`。这表示套件与交付入口已经到位，但当前代码包里真实运行件仍未闭环。

## 5. 回滚方案

- 保留 D15 baseline `.config` 作为总回退点。
- 每轮裁剪分阶段保存 fragment、结果文档和 records，不把多轮改动揉成一次不可拆的变化。
- rootfs 继续保留 BusyBox 基线版本；遇到 `perf` 相关问题时，优先区分是内核配置问题还是 rootfs 工具缺失。
- 出现异常时，先回退到 baseline 的 config + rootfs + QEMU 启动参数，不要同时修改内核和 rootfs 再重新验证。
- 如果 Day20 `latest_verdict=MISSING_INPUTS`，优先按 Day20 的 latest / verify 路径排查。当前缺失输入件为：`image,rootfs,dtb`。

## 6. 当前最推荐动作

- **要继续交付**：直接复用 Day19 的量化表与 Day20 的交付状态，把这份 Day21 报告作为 W3 的压缩版结论入口。
- **要继续验证**：先补齐 `Image/rootfs/dtb`，再用 Day20 套件执行真实回归，把 `RUNTIME_READY` 和 `REGRESSION_PASS` 推到可验证状态。
- **要继续演进**：优先补齐 D15 / D16 的结构化 records，再回刷 Day19 / Day21 报告，让跨阶段口径更硬。
