# Day18 原始需求分析

## 1. 原始需求来源

Day18 的原始需求并不是来自现成的 `day18/README.md`，因为当前上传基线里还没有 day18 目录。它主要来自：

- `docs/W3_REVIEW.md`
- `docs/ROADMAP.md`
- `day15/README.md` 中 W3 路线说明
- `day17/README.md` 对后续 day18/day19 的承接

其中最关键的是 `docs/W3_REVIEW.md` 对 D18 的定义：

- D16：去明显无关项的粗裁
- D18：按类别整理的可解释裁剪

## 2. Day18 真正要解决的问题

Day18 不是继续无脑改 `.config`，而是解决三个工程化问题：

### 2.1 为什么保留这些项

如果只会说“为了能启动”“为了 demo 能跑”，说明配置知识还是碎片化的。
Day18 要求把保留项按角色说清楚：

- 这是系统启动必须项
- 这是 arm64/QEMU virt 平台项
- 这是调试项
- 这是 perf 项

### 2.2 为什么删除这些项

删项也不能只靠经验，要能明确说明：

- 当前实验路径是否真的不依赖它
- 它对启动链、demo_regmap、串口采样、perf/ftrace 是否有影响

### 2.3 怎么让后续回滚和汇报更容易

Day18 的分类结果，最终要服务于：

- D19 的数据对比
- D20 的自动回归
- D21 的最终报告

所以它必须是可归档、可追溯、可对比的。

## 3. Day18 和 Day16 的本质区别

### Day16

- 目标：先粗裁
- 特征：去明显无关项
- 风险：很多删项只停留在经验层面

### Day18

- 目标：第二轮分类裁剪
- 特征：分类管理、可解释、可回滚
- 关键价值：让配置选择具备“能讲清楚”的结构

## 4. 为什么 Day18 要独立目录

如果 Day18 继续混在 day17 里，就会有两个问题：

- day17 的“收口版”定位被破坏
- day18 的“分类裁剪”价值会被淹没成只是多加几份 fragment

所以更合理的做法是：

- day17：保持为收口后的独立实验工作台
- day18：独立出来，专门承载第二轮分类裁剪

## 5. 推荐技术路线

### 第一步：保留 day17 的可运行骨架

不要推倒重来，继续复用：

- apply_config/build/run_qemu/collect 链
- perf 集成链
- evidence 链

### 第二步：引入分类 fragment

把 day18 目标分成：

- required
- platform
- debug
- perf
- trim

### 第三步：保留一个 legacy 对照

这样能回答两个问题：

- day18 的分类表达和 day17 的 round2b 是否等价
- 如果不等价，差异到底来自哪里

## 6. Day18 最终应该做成什么样

建议最终至少具备：

- 可独立阅读的 `README.md`
- 可独立执行的脚本链
- 分类 fragment
- 分类总览表
- `.config` + `savedefconfig` 证据
- baseline / legacy / classified 三轮对照

## 7. 学习重点

### 配置层

- fragment / olddefconfig / savedefconfig
- 顶层开关与子项联动

### 平台层

- arm64
- QEMU virt
- PL011
- GIC
- Device Tree

### 设备层

- platform_driver
- of_match
- irqdomain
- regmap

### 观测层

- debugfs
- ftrace
- function_graph
- perf

### 工程化层

- baseline / legacy / classified
- evidence
- compare
- rollback

## 8. 验收建议

Day18 的验收至少分四层：

- 启动与功能是否通过
- `.config` / `savedefconfig` / `Image` 是否可追溯
- 分类是否可解释
- legacy 与 classified 是否可对照
