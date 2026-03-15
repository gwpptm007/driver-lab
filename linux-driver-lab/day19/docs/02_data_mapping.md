# Day19 数据来源映射

## 1. 为什么要单独做这一页

Day19 要把 D15 / D16 / D18 串成同一份对比报告，但当前仓库里三阶段的数据沉淀方式并不完全一致：

- D15、D16 主要以结果文档沉淀
- D18 已经进入 `records + compare + evidence` 的标准化方式

所以 Day19 必须把“每个字段从哪里来”写清楚，否则表格虽然能生成，但口径会发虚。

---

## 2. 对外阶段映射

Day19 对外仍然按 `docs/W3_REVIEW.md` 的定义表达：

- **baseline = D15**
- **trim1 = D16**
- **trim2 = D18**

这里的关键是：**对外按阶段写，对内按已有产物取数。**

---

## 3. 当前采用的具体取数点

### 3.1 baseline（D15）

当前主来源：

- `day15/RESULTS.md`

当前直接抽取字段：

- `Image` 大小
- `rootfs.img` 大小
- `boot_ms`
- `MemTotal`
- `MemFree`
- `MemAvailable`
- `Slab`
- `modules_loaded_count_after`
- `function_graph_ok`
- `perf_bin_ok`

说明：

- D15 结果文档里没有把 `modules_built_count` 明确列在结果表中，因此 Day19 草稿版先记为缺省，报告里单独说明。

### 3.2 trim1（D16）

当前主来源：

- `day16/RESULTS_ROUND1.md`

辅助说明来源：

- `day16/RESULTS_SUMMARY.md`
- `day16/RESULTS_ROUND2B.md`

当前直接抽取字段：

- `image_bytes`
- `boot_ms`
- `memfree_kib`
- `memavailable_kib`
- `slab_kib`
- 功能回归项（`function_graph_ok`、`insmod_ok` 等）

说明：

- Day16 目录内既有 `round1`，也有 `round2b`。
- Day19 这版对外先把 **D16 round1 作为 trim1**，因为 `W3_REVIEW.md` 对 D16 的定义就是“第一轮粗裁”。
- `round2b` 继续保留在报告解释里，作为“Day16 内部增强版收口”的旁证，不直接占用 trim1 位置。

### 3.3 trim2（D18）

当前主来源：

- `day18/records/compare-20260315-142441.csv`

辅助来源：

- `day18/records/compare-20260315-142441.md`
- `day18/records/equivalence-round2b_legacy-vs-classified.txt`
- `day18/records/LAST_classified.txt`

当前直接抽取字段：

- `boot_ms`
- `image_kib`
- `rootfs_kib`
- `modules_built_count`
- `modules_loaded_count`
- `MemTotal`
- `MemFree`
- `MemAvailable`
- `Slab`
- `function_graph_ok`
- `perf_bin_ok`
- `perf_smoke_ok`
- `pass_status`

说明：

- Day19 选择 **classified** 作为 trim2 的代表项。
- `round2b_legacy` 不单独占用 trim2 位置，而是作为“分类表达是否只是说法升级”的等价性旁证。
- 当前等价性文件明确给出：`round2b_legacy` 与 `classified` 的 `kernel.config sha256` 相同。

---

## 4. 当前不完全同口径的地方

这是 Day19 草稿版最需要正视的一点。

### 4.1 D15 / D16 与 D18 的记录形态不同

- D15、D16：偏结果文档
- D18：偏标准化 records

这意味着某些字段（如 `modules_built_count`）在前两阶段没有显式落表，只能保守留空或写成“未显式记录”。

### 4.2 D18 已处于新的 rootfs/perf 周期

从 D17 开始，rootfs 已不再只是最早的 BusyBox 最小形态，而是进入了“带 perf 工具”的新周期。

因此：

- D18 的 `rootfs.img` 体积不能简单和 D15/D16 做“同口径收益”解释
- D18 的 `boot_ms` 也需要带着“周期变化”的说明来读

### 4.3 Day19 当前最稳的读法

所以这版 Day19 建议这样读：

- **D15 vs D16**：可以认真讲“第一轮粗裁的量化收益”
- **D18**：数字可以进表，但更适合作为“当前最终阶段的状态与风险旁证”，而不是直接拿来和 D15/D16 做无条件横向排名

---

## 5. 当前结论

Day19 这版数据映射的核心原则是：

> **先把来源写清楚，再谈结论；先接受“草稿版的口径边界”，再决定要不要补统一重采样。**
