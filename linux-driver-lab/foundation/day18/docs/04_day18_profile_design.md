# Day18 profile 设计

## 1. baseline

用途：

- 做所有 compare 的基线
- 提供 tracing / perf / demo_regmap 的最低可用环境

## 2. round2b_legacy

用途：

- 保留 day17 的连续性
- 给 day18 的 classified 提供对照组

fragment 链：

- trace_baseline.fragment
- trim_round1.fragment
- trim_round2b.fragment

## 3. classified

用途：

- 用分类方式表达第二轮裁剪
- 让“为什么保留 / 为什么删除”清晰可讲

fragment 链：

- trace_baseline.fragment
- 10_required.fragment
- 20_platform.fragment
- 30_debug.fragment
- 40_perf.fragment
- 90_trim_day18.fragment

## 4. 设计原则

- baseline 只负责最小可观测性
- legacy 负责连续性
- classified 负责解释性
