# Day19 - 对比报告草稿与交付版收口

## 1. Day19 的原始任务

Day19 来自 `linux-driver-lab/docs/W3_REVIEW.md`：

- 输出对比：`size / boot / mem / module 数`
- 列风险项
- 产出：**对比报告草稿**

当前这版已经不是“只立计划”，而是已经形成了 **首版可交付收口**：

- 有计划文档
- 有数据映射与指标定义
- 有风险与验收文档
- 有数据来源清单
- 有报告生成脚本
- 有 `csv / draft md / final md / risk matrix / summary / acceptance checklist` 产物

---

## 2. 当前目录里有什么

### 入口文件

- `README.md`
- `START_HERE.md`

### 过程文档

- `docs/01_day19_plan.md`
- `docs/02_data_mapping.md`
- `docs/03_metric_definition.md`
- `docs/04_risk_items.md`
- `docs/05_acceptance.md`
- `docs/06_next_steps.md`

### 生成逻辑

- `source_manifest.csv`
- `generate_day19_report.py`
- `run_day19_report.sh`

### 当前产物

- `output/day19_compare_table.csv`
- `output/day19_compare_report_draft.md`
- `output/day19_compare_report_final.md`
- `output/day19_risk_matrix.md`
- `output/day19_summary.txt`
- `output/day19_acceptance_checklist.md`

---

## 3. 当前这版最重要的读法

Day19 这版最重要的不是“表已经有了”，而是要知道**哪些结论最稳、哪些结论必须带 caveat**：

- **D15 baseline ↔ D16 trim1(round1)**：当前最稳的直接收益对比
- **D18 trim2(classified)**：当前最终阶段状态与方法旁证，可入表，但必须带“rootfs/perf 周期变化”的说明

所以现在最推荐的阅读口径是：

> **最稳的量化收益看 D15 -> D16；当前最终状态与方法表达看 D18。**

---

## 4. 如何重新生成当前产物

在 `linux-driver-lab/day19` 下执行：

```bash
./run_day19_report.sh
```

它会重新生成：

- `output/day19_compare_table.csv`
- `output/day19_compare_report_draft.md`
- `output/day19_compare_report_final.md`
- `output/day19_risk_matrix.md`
- `output/day19_summary.txt`
- `output/day19_acceptance_checklist.md`

---

## 5. 建议先看什么

第一次进入 Day19，建议按这个顺序看：

1. `START_HERE.md`
2. `docs/01_day19_plan.md`
3. `docs/02_data_mapping.md`
4. `docs/03_metric_definition.md`
5. `output/day19_summary.txt`
6. `output/day19_compare_report_final.md`
7. `output/day19_risk_matrix.md`
8. `output/day19_acceptance_checklist.md`

如果只想先看结论，直接打开：

- `output/day19_summary.txt`
- `output/day19_compare_report_final.md`
