# Day19 后续动作建议

## 1. 当前 Day19 已经完成什么

Day19 当前已经完成了“首版对比报告草稿”的工程化落地，并且已经进一步收成了可交付口吻：

- 已有独立目录
- 已有计划、数据映射、指标定义、风险、验收文档
- 已有 `source_manifest.csv`
- 已有报告生成脚本
- 已有 `csv + draft md + final md + risk matrix + summary + acceptance checklist` 输出

这意味着 D19 原始任务已经进入“可交付草稿”状态。

---

## 2. 后续最值得继续做的三件事

### 2.1 补统一 records（增强项）

当前 D15 / D16 更多来自结果文档，D18 来自标准化 records。

如果后面要让 Day19 的量化结论更“硬”，最值得做的是：

- 为 D15 补齐和 D18 同风格的 records
- 为 D16 round1 补齐和 D18 同风格的 records
- 让 Day19 报告更多依赖结构化输入，而不是文档抽取

### 2.2 让 Day20 自动回归直接复用 Day19

Day19 当前已经把关键字段组织成了统一表头。后续 Day20 可以直接复用：

- `image_kib`
- `boot_ms`
- `memfree_kib`
- `slab_kib`
- `modules_loaded_count`
- `function_graph_ok`
- `perf_ok`

这样回归跑完后，可以自动更新 Day19 的 compare table。

### 2.3 为 D21 输出更正式的交付稿

Day19 当前更像“技术草稿版 + 交付版正文”。

到 D21 时，可以基于 Day19 再收敛成：

- 1 页摘要版
- 1 份正式结论版
- 1 份对外讲解版

---

## 3. 如果只继续做一步，优先做什么

如果只做一步增强，我建议优先做：

> **补一版更统一口径的 D15 / D16 records，再让 Day19 报告重生成一次。**

因为这一步最直接提升 Day19 报告的可信度。
