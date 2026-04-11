# Day19 验收口径

## 1. 当前 Day19 要验什么

D19 的原始要求是：

- 输出对比：`size / boot / mem / module 数`
- 列风险项
- 产出：对比报告草稿

所以 Day19 的验收，不是看它写了多少文件，而是看它有没有把这三件事真正落地。

---

## 2. 建议的验收项

### 2.1 目录独立

Day19 是否已经拥有自己的：

- `README.md`
- `START_HERE.md`
- `docs/`
- `output/`
- 生成脚本

### 2.2 数据来源清楚

是否已经明确：

- baseline 从哪里来
- trim1 从哪里来
- trim2 从哪里来
- 哪些字段是直接取数
- 哪些字段带口径 caveat

### 2.3 对比表已生成

是否已经产出：

- `output/day19_compare_table.csv`

并且表里至少覆盖：

- `Image`
- `rootfs.img`
- `boot_ms`
- `MemFree`
- `Slab`
- `module 数`
- `function_graph`
- `perf`
- 风险备注

### 2.4 报告草稿已生成

是否已经产出：

- `output/day19_compare_report_draft.md`

并且能回答：

- baseline / trim1 / trim2 分别是谁
- 哪些数字能直接比较
- 哪些数字要带 caveat
- 当前风险在哪里

---

## 3. 当前这一版的验收结论建议

如果本目录已经具备：

- 计划文档
- 数据映射文档
- 指标定义文档
- 风险文档
- 生成脚本
- CSV 对比表
- Markdown 报告草稿

那么就可以认为：

> **Day19 第一版已经完成“对比报告草稿”的工程化落地。**

这时仍未完成的，不是 D19 本身，而是“是否要进一步增强成更严格统一口径版”。
