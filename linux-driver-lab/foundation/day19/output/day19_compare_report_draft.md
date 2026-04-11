# Day19 对比报告草稿

## 1. 摘要结论

这版 Day19 已经把 `D15 / D16 / D18` 三阶段串成了同一份对比草稿，但读法要分层：

- **最稳的直接收益结论**：看 `D15 baseline` 与 `D16 trim1(round1)`。
- **当前最终阶段的状态与方法旁证**：看 `D18 trim2(classified)`，但它已经进入新的 rootfs/perf 周期，数字入表时必须带 caveat。

当前最值得直接写进结论区的量化结果是：

- `D15 -> D16 trim1`：`Image` 从 `38867 KiB` 降到 `37237 KiB`，减少 `1630 KiB`（-4.2%）；`boot_ms` 仅变化 `+13 ms`；`MemFree` 与 `Slab` 都有轻微改善。
- `D18 trim2(classified)`：当前 profile 运行通过、`function_graph` 与 `perf` 都可用，且 `classified` 与 `round2b_legacy` 的 `kernel.config sha256` 一致；但其 rootfs/boot 已受新周期影响，不应直接拿来和 D15 / D16 做“纯收益排名”。

---

## 2. 阶段映射

- **baseline** = `D15 baseline`
- **trim1** = `D16 round1`
- **trim2** = `D18 classified`

说明：

- `D16` 目录内部还存在 `round2b`，但那属于 Day16 内部增强版收口；Day19 对外仍先把 `round1` 作为 trim1。
- `D18 round2b_legacy` 不单独占 trim2 位置，而是作为 `classified` 的等价性旁证。

---

## 3. 关键指标总表

| 阶段 | Image KiB | ΔImage | rootfs KiB | Δrootfs | boot ms | Δboot | MemFree KiB | ΔMemFree | Slab KiB | ΔSlab | 模块构建数 | 运行时模块数 | function_graph | perf | 备注 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|---|
| D15 baseline | 38867 | 0 | 1181 | 0 | 2008 | 0 | 968564 | 0 | 12252 | 0 | n/a | 1 | yes | no | 可作为 Day19 的起点基线；perf 用户态尚未进入 rootfs。 |
| D16 trim1 (round1) | 37237 | -1630 | 1181 | 0 | 2021 | +13 | 969716 | +1152 | 12108 | -144 | n/a | 1 | yes | kernel-side-kept,userland-not-shown | D16 内部还有 round2b 增强版，但 Day19 对外口径先把 round1 作为 trim1。 |
| D18 trim2 (classified) | 27417 | -11450 | 8128 | +6947 | 2054 | +46 | 961808 | -6756 | 8236 | -4016 | 2 | 1 | yes | yes/yes | rootfs 已含 perf，boot 与 rootfs 体积都受 D17/D18 周期变化影响；更适合作为当前最终形态和方法旁证。 |

---

## 4. 数据来源与口径说明

### 4.1 D15 baseline

主来源：`day15/RESULTS.md`

特点：

- 提供了 `Image / rootfs / boot / mem / function_graph / perf` 等关键字段
- `modules_loaded_count_after=1` 明确可读
- `modules_built_count` 在结果文档里未显式落表

### 4.2 D16 trim1

主来源：`day16/RESULTS_ROUND1.md`

特点：

- `image_bytes / boot_ms / memfree_kib / slab_kib` 等字段直接可用
- 功能回归项明确说明 `function_graph` 与 demo 模块链路仍然正常
- 结果文档未单独列出 `modules_built_count`

### 4.3 D18 trim2

主来源：`day18/records/compare-20260315-142441.csv`

旁证：

- `compare-20260315-142441.md`
- `equivalence-round2b_legacy-vs-classified.txt`

特点：

- 字段最完整，连 `modules_built_count`、`perf_smoke_ok`、sha256 证据链都有
- `classified` 与 `round2b_legacy` 的 `kernel.config sha256` 相同，说明 D18 更像“表达升级 + 结果保持一致”

---

## 5. 关键变化解读

### 5.1 D15 -> D16：第一轮粗裁的直接收益

这是当前 Day19 最能放心讲的一段：

- `Image`：`38867 KiB -> 37237 KiB`，减少 `1630 KiB`，约 `-4.2%`
- `boot_ms`：`2008 -> 2021`，变化 `+13 ms`，可视为基本持平
- `MemFree`：`968564 -> 969716`，增加 `+1152 KiB`
- `Slab`：`12252 -> 12108`，变化 `-144 KiB`

这说明：

> 第一轮粗裁确实带来了镜像缩小，而且没有以牺牲启动链路、demo 模块、`function_graph` 为代价。

### 5.2 D18：更适合作为“当前终态”而不是“无 caveat 的横比项”

D18 classified 当前数据是：

- `Image = 27417 KiB`
- `rootfs = 8128 KiB`
- `boot_ms = 2054`
- `function_graph = yes`
- `perf = yes/yes`

但这里不能直接写成“D18 一定比 D15 / D16 更优”，原因有两个：

1. `rootfs` 已进入带 perf 的新周期，体积与 boot 行为都变了。
2. `D18` 的价值更强地体现在：`classified` 与 `round2b_legacy` 最终 `kernel.config` 等价，说明第二轮分类裁剪的方法表达成立。

所以，Day19 当前对 D18 更准确的表述应该是：

> D18 证明了“分类表达 + 当前 profile 可运行 + perf 已可用”；至于跨周期的纯量化排序，后续如需更严谨，需要补统一口径重采样。

---

## 6. 当前风险项

### 6.1 口径统一风险

D15 / D16 主要来自结果文档，D18 来自标准化 records。表已经能出，但严格程度仍然是“草稿版”。

### 6.2 rootfs/perf 周期变化风险

D18 的 `rootfs_kib` 和 `boot_ms` 已受到 D17 之后新工具集的影响，因此不能无条件与 D15 / D16 直接横向排名。

### 6.3 平台扩展风险

当前结论建立在 `arm64 + QEMU virt + 当前 demo` 上，后续扩展到更多 virtio / PCIe / 真板外设时，今天被裁掉的项可能需要补回。

### 6.4 观测能力保留风险

如果只盯着继续缩 `Image`，最容易先把 `DEBUG_FS / FTRACE / FUNCTION_GRAPH / PERF_EVENTS` 一起裁掉，这会直接破坏 W3 的目标。

---

## 7. 验收视角下的当前结论

站在 D19 原始验收口径上，这版 Day19 已经满足“对比报告草稿”的核心目标：

- 三阶段表已经形成
- 风险项已经单独列出
- 能直接讲的结论和必须带 caveat 的结论已经分开

当前最稳的一句话结论是：

> **D15 baseline 到 D16 trim1 的第一轮粗裁收益已经有明确量化证据；D18 trim2 则更强地证明了当前最终阶段的 profile 可运行、classified 表达成立，并把 perf 带进了新周期。**
