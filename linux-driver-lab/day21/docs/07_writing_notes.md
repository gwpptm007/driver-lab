# Day21 写作收口说明

## 1. 这一版 Day21 往前推进了什么

Day21 这一步不再停留在“计划和模板”，而是正式生成了三类输出：

- `output/day21_report_draft.md`
- `output/day21_report_final.md`
- `output/day21_report_onepager.md`

同时增加：

- `run_day21_report.sh`
- `generate_day21_report.py`
- `source_manifest.csv`
- `output/day21_data_snapshot.csv`
- `output/day21_acceptance_checklist.md`

## 2. 当前写作原则

### 原则一：短报告只讲主线

Day21 不重新展开 Day15-Day20 的所有细节，只保留：

- 背景
- 方法
- 核心数据
- 结论
- 回滚方案

### 原则二：最稳结论和带边界结论分开写

当前最稳的是 D15 → D16 的第一轮粗裁收益；D18 可以进表，但必须带 rootfs/perf 周期变化说明。

### 原则三：Day20 状态要写得诚实

当前 Day20 的套件已经成熟，但这份代码包里运行件未齐，所以不能写成“最终真实回归已 PASS”。

## 3. 这一版 Day21 的定位

这一版已经不是纯规划稿，而是：

> **正式报告初稿 + 正式报告版 + 一页版同时成立。**

后面如果继续收，只需要继续做文字压缩和数据口径增强，而不是重新设计 Day21 目录。
