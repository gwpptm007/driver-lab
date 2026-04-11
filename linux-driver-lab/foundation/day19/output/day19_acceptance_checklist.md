# Day19 验收清单

## 1. 目录与产物

- [x] `day19/` 已独立成目录
- [x] 已提供 `README.md` 与 `START_HERE.md`
- [x] 已提供 `docs/` 过程文档
- [x] 已提供 `source_manifest.csv`
- [x] 已提供 `run_day19_report.sh` 与 `generate_day19_report.py`
- [x] 已生成 `day19_compare_table.csv`
- [x] 已生成 `day19_compare_report_draft.md`
- [x] 已生成 `day19_compare_report_final.md`
- [x] 已生成 `day19_risk_matrix.md` 与 `day19_summary.txt`

## 2. D19 原始任务覆盖情况

| 原始要求 | 当前状态 | 说明 |
|---|---|---|
| 输出 size 对比 | 已覆盖 | `Image` 与 `rootfs` 均入表 |
| 输出 boot 对比 | 已覆盖 | `boot_ms` 入表，并在正文解释可比性 |
| 输出 mem 对比 | 已覆盖 | `MemFree`、`Slab` 为主解读字段 |
| 输出 module 数 | 已覆盖但不完全对称 | `modules_loaded_count` 三阶段都有；`modules_built_count` 仅 D18 明确 |
| 列风险项 | 已覆盖 | 风险矩阵单独成文 |
| 形成对比报告草稿 | 已覆盖 | draft 与 final 两版都已生成 |

## 3. 当前最稳的结论

- `D15 -> D16`：`Image` 从 `38867 KiB` 降到 `37237 KiB`，减少 `1630 KiB`。
- `D15 -> D16`：`boot_ms` 从 `2008` 变到 `2021`，仅 `+13 ms`。
- `D15 -> D16`：`MemFree` 增加 `+1152 KiB`，`Slab` 下降 `144 KiB`。
- `D18 trim2`：当前 `function_graph=yes`，`perf=yes/yes`，说明最终阶段 profile 可运行，但需保留跨周期 caveat。

## 4. 当前验收判断

> **结论：Day19 已完成“对比报告草稿”的交付目标。**

补充说明：

- 这版已经足够作为 D19 的工程化交付。
- 若后续要把结论再做硬，就继续补 D15 / D16 结构化 records。
