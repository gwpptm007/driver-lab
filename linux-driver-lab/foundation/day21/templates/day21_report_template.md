# W3 最终总结报告（Day21 模板）

## 1. 背景

W3 的目标不是继续增加新的驱动功能，而是把已经跑通的 arm64 + QEMU virt 实验环境，收敛成一套可裁剪、可对比、可回归、可回滚的最小内核实验平台。为此，本阶段围绕 baseline 冻结、两轮裁剪、量化对比和自动回归四条主线推进，并在保留 tracing / perf 基础能力的前提下完成工程化收口。

## 2. 方法

- D15：冻结 baseline 配置、启动参数和验证对象
- D16：执行第一轮粗裁，去掉明显无关项但保留 tracing / perf / debug 能力
- D17 / D18：明确 rootfs 路线，并完成第二轮分类裁剪与分类表达整理
- D19：整理 baseline / trim1 / trim2 的 size / boot / mem / module 数对比
- D20：建立自动回归套件，补齐 smoke / trace / perf / stress 与最新结果/交付状态入口

## 3. 核心数据

| 阶段 | Image | rootfs.img | boot | mem | module 数 | 备注 |
|---|---:|---:|---:|---:|---:|---|
| baseline | TBD | TBD | TBD | TBD | TBD | 基线 |
| trim1 | TBD | TBD | TBD | TBD | TBD | 第一轮粗裁 |
| trim2 | TBD | TBD | TBD | TBD | TBD | 第二轮分类裁剪 |

> 说明：如果不同阶段存在 rootfs/perf 周期变化，必须在报告正文中补一句口径边界说明。

## 4. 结论

- 当前 W3 已形成 baseline → trim1 → trim2 → compare → regression 的完整收口链路
- 量化结果表明，裁剪后在镜像/启动/内存/模块数量上获得了可观测收益（具体数值待填）
- tracing / perf 相关能力在推荐配置中被明确保留
- 当前最推荐的组合是：`arm64 + QEMU virt + BusyBox rootfs + demo_regmap.ko + debugfs/ftrace/function_graph/perf basic`
- D20 已具备交付级回归套件骨架；若真实运行件缺失，应明确表述为“套件成熟，但当前包未完成真实回归输入件闭环”

## 5. 回滚方案

- 保留 baseline `.config`
- 每轮裁剪单独 commit / tag
- rootfs 保留 BusyBox 基线版本
- 出现异常先回退到 baseline 的 config + rootfs + QEMU 启动参数
- perf 异常排查时，不要同时修改内核和 rootfs

## 6. 当前推荐动作

- 如果要继续交付：基于 Day19 / Day20 已有产物填充正式最终版数据与结论
- 如果要继续验证：补齐 Image / rootfs / dtb 等运行件后，用 Day20 套件执行真实回归
