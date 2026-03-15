#!/usr/bin/env python3
from __future__ import annotations

import csv
from pathlib import Path

DAY19 = Path(__file__).resolve().parent
MANIFEST = DAY19 / 'source_manifest.csv'
OUT_DIR = DAY19 / 'output'
OUT_CSV = OUT_DIR / 'day19_compare_table.csv'
OUT_DRAFT = OUT_DIR / 'day19_compare_report_draft.md'
OUT_FINAL = OUT_DIR / 'day19_compare_report_final.md'
OUT_RISK = OUT_DIR / 'day19_risk_matrix.md'
OUT_SUMMARY = OUT_DIR / 'day19_summary.txt'
OUT_ACCEPT = OUT_DIR / 'day19_acceptance_checklist.md'


def load_rows() -> list[dict[str, str]]:
    with MANIFEST.open(encoding='utf-8', newline='') as f:
        return list(csv.DictReader(f))


def to_int(row: dict[str, str], key: str, default: int = 0) -> int:
    value = (row.get(key) or '').strip()
    if value == '':
        return default
    return int(value.replace('+', ''))


def percent_change(old: int, new: int) -> str:
    if old == 0:
        return 'n/a'
    delta = (new - old) / old * 100
    sign = '+' if delta > 0 else ''
    return f'{sign}{delta:.1f}%'


def write_csv(rows: list[dict[str, str]]) -> None:
    fields = [
        'stage', 'stage_label', 'config_name',
        'image_kib', 'delta_image_kib_vs_baseline',
        'rootfs_kib', 'delta_rootfs_kib_vs_baseline',
        'boot_ms', 'delta_boot_ms_vs_baseline',
        'memfree_kib', 'delta_memfree_kib_vs_baseline',
        'slab_kib', 'delta_slab_kib_vs_baseline',
        'modules_built_count', 'modules_loaded_count',
        'function_graph_ok', 'perf_ok', 'pass_status',
        'source_type', 'compare_scope', 'comparable_note',
        'risk_note', 'source_primary', 'evidence_note',
    ]
    with OUT_CSV.open('w', encoding='utf-8', newline='') as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        for row in rows:
            w.writerow({k: row.get(k, '') for k in fields})


def render_table(rows: list[dict[str, str]]) -> str:
    lines = [
        '| 阶段 | Image KiB | ΔImage | rootfs KiB | Δrootfs | boot ms | Δboot | MemFree KiB | ΔMemFree | Slab KiB | ΔSlab | 模块构建数 | 运行时模块数 | function_graph | perf | 备注 |',
        '|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|---|',
    ]
    for r in rows:
        lines.append(
            f"| {r['stage_label']} | {r['image_kib'] or 'n/a'} | {r['delta_image_kib_vs_baseline'] or 'n/a'} | "
            f"{r['rootfs_kib'] or 'n/a'} | {r['delta_rootfs_kib_vs_baseline'] or 'n/a'} | "
            f"{r['boot_ms'] or 'n/a'} | {r['delta_boot_ms_vs_baseline'] or 'n/a'} | "
            f"{r['memfree_kib'] or 'n/a'} | {r['delta_memfree_kib_vs_baseline'] or 'n/a'} | "
            f"{r['slab_kib'] or 'n/a'} | {r['delta_slab_kib_vs_baseline'] or 'n/a'} | "
            f"{r['modules_built_count'] or 'n/a'} | {r['modules_loaded_count'] or 'n/a'} | "
            f"{r['function_graph_ok'] or 'n/a'} | {r['perf_ok'] or 'n/a'} | {r['risk_note']} |"
        )
    return '\n'.join(lines)


def write_risk_matrix() -> None:
    risk = """# Day19 风险矩阵

| 风险类别 | 当前表现 | 影响面 | 当前判断 | 建议动作 |
|---|---|---|---|---|
| 口径统一风险 | D15/D16 主要来自结果文档，D18 来自标准化 records | 影响跨阶段“严格同比”结论 | 中 | 后续补齐 D15/D16 结构化 records |
| rootfs/perf 周期变化 | D18 已进入带 perf 的 rootfs 周期 | 影响 rootfs 体积与 boot 横比解释 | 高 | 报告中保留 caveat；需要时补统一重采样 |
| 平台扩展风险 | 当前验证建立在 arm64 + QEMU virt + 当前 demo 上 | 影响未来扩到 PCIe/virtio/真板时的可复用性 | 中 | 后续新平台 bring-up 时重新做裁剪回归 |
| 观测能力保留风险 | 继续极限裁剪容易误伤 debugfs/ftrace/perf | 影响 W3 学习和分析目标 | 高 | 在 D20/D21 中把 function_graph/perf 继续列为硬验收项 |
| module 数字段不完整 | D15/D16 未显式落 `modules_built_count` | 影响“module 数”维度的完整度 | 中 | 后续补 records 或补构建统计脚本 |
"""
    OUT_RISK.write_text(risk, encoding='utf-8')


def write_summary(rows: list[dict[str, str]]) -> None:
    baseline, trim1, trim2 = rows
    image_delta = trim1['delta_image_kib_vs_baseline']
    boot_delta = trim1['delta_boot_ms_vs_baseline']
    mem_delta = trim1['delta_memfree_kib_vs_baseline']
    lines = [
        'Day19 summary',
        '=============',
        '',
        f"baseline : {baseline['stage_label']}  image={baseline['image_kib']} KiB  boot={baseline['boot_ms']} ms  memfree={baseline['memfree_kib']} KiB",
        f"trim1    : {trim1['stage_label']}  image={trim1['image_kib']} KiB  boot={trim1['boot_ms']} ms  memfree={trim1['memfree_kib']} KiB",
        f"trim2    : {trim2['stage_label']}  image={trim2['image_kib']} KiB  boot={trim2['boot_ms']} ms  memfree={trim2['memfree_kib']} KiB",
        '',
        f'D15 -> D16 image delta : {image_delta} KiB',
        f'D15 -> D16 boot delta  : {boot_delta} ms',
        f'D15 -> D16 mem delta   : {mem_delta} KiB',
        '',
        'Interpretation:',
        '- D15 -> D16 is the most stable same-context comparison.',
        '- D18 can be included in the table, but boot/rootfs must be read with a perf-cycle caveat.',
    ]
    OUT_SUMMARY.write_text('\n'.join(lines) + '\n', encoding='utf-8')


def write_acceptance(rows: list[dict[str, str]]) -> None:
    baseline, trim1, trim2 = rows
    lines = [
        '# Day19 验收清单',
        '',
        '## 1. 目录与产物',
        '',
        '- [x] `day19/` 已独立成目录',
        '- [x] 已提供 `README.md` 与 `START_HERE.md`',
        '- [x] 已提供 `docs/` 过程文档',
        '- [x] 已提供 `source_manifest.csv`',
        '- [x] 已提供 `run_day19_report.sh` 与 `generate_day19_report.py`',
        '- [x] 已生成 `day19_compare_table.csv`',
        '- [x] 已生成 `day19_compare_report_draft.md`',
        '- [x] 已生成 `day19_compare_report_final.md`',
        '- [x] 已生成 `day19_risk_matrix.md` 与 `day19_summary.txt`',
        '',
        '## 2. D19 原始任务覆盖情况',
        '',
        '| 原始要求 | 当前状态 | 说明 |',
        '|---|---|---|',
        '| 输出 size 对比 | 已覆盖 | `Image` 与 `rootfs` 均入表 |',
        '| 输出 boot 对比 | 已覆盖 | `boot_ms` 入表，并在正文解释可比性 |',
        '| 输出 mem 对比 | 已覆盖 | `MemFree`、`Slab` 为主解读字段 |',
        '| 输出 module 数 | 已覆盖但不完全对称 | `modules_loaded_count` 三阶段都有；`modules_built_count` 仅 D18 明确 |',
        '| 列风险项 | 已覆盖 | 风险矩阵单独成文 |',
        '| 形成对比报告草稿 | 已覆盖 | draft 与 final 两版都已生成 |',
        '',
        '## 3. 当前最稳的结论',
        '',
        f'- `D15 -> D16`：`Image` 从 `{baseline["image_kib"]} KiB` 降到 `{trim1["image_kib"]} KiB`，减少 `{abs(to_int(trim1, "delta_image_kib_vs_baseline"))} KiB`。',
        f'- `D15 -> D16`：`boot_ms` 从 `{baseline["boot_ms"]}` 变到 `{trim1["boot_ms"]}`，仅 `{trim1["delta_boot_ms_vs_baseline"]} ms`。',
        f'- `D15 -> D16`：`MemFree` 增加 `{trim1["delta_memfree_kib_vs_baseline"]} KiB`，`Slab` 下降 `{abs(to_int(trim1, "delta_slab_kib_vs_baseline"))} KiB`。',
        f'- `D18 trim2`：当前 `function_graph={trim2["function_graph_ok"]}`，`perf={trim2["perf_ok"]}`，说明最终阶段 profile 可运行，但需保留跨周期 caveat。',
        '',
        '## 4. 当前验收判断',
        '',
        '> **结论：Day19 已完成“对比报告草稿”的交付目标。**',
        '',
        '补充说明：',
        '',
        '- 这版已经足够作为 D19 的工程化交付。',
        '- 若后续要把结论再做硬，就继续补 D15 / D16 结构化 records。',
    ]
    OUT_ACCEPT.write_text('\n'.join(lines) + '\n', encoding='utf-8')


def write_draft(rows: list[dict[str, str]]) -> None:
    baseline, trim1, trim2 = rows
    image_drop = abs(to_int(trim1, 'delta_image_kib_vs_baseline'))
    image_pct = percent_change(to_int(baseline, 'image_kib'), to_int(trim1, 'image_kib'))
    report = f"""# Day19 对比报告草稿

## 1. 摘要结论

这版 Day19 已经把 `D15 / D16 / D18` 三阶段串成了同一份对比草稿，但读法要分层：

- **最稳的直接收益结论**：看 `D15 baseline` 与 `D16 trim1(round1)`。
- **当前最终阶段的状态与方法旁证**：看 `D18 trim2(classified)`，但它已经进入新的 rootfs/perf 周期，数字入表时必须带 caveat。

当前最值得直接写进结论区的量化结果是：

- `D15 -> D16 trim1`：`Image` 从 `{baseline['image_kib']} KiB` 降到 `{trim1['image_kib']} KiB`，减少 `{image_drop} KiB`（{image_pct}）；`boot_ms` 仅变化 `{trim1['delta_boot_ms_vs_baseline']} ms`；`MemFree` 与 `Slab` 都有轻微改善。
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

{render_table(rows)}

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

- `Image`：`{baseline['image_kib']} KiB -> {trim1['image_kib']} KiB`，减少 `{image_drop} KiB`，约 `{image_pct}`
- `boot_ms`：`{baseline['boot_ms']} -> {trim1['boot_ms']}`，变化 `{trim1['delta_boot_ms_vs_baseline']} ms`，可视为基本持平
- `MemFree`：`{baseline['memfree_kib']} -> {trim1['memfree_kib']}`，增加 `{trim1['delta_memfree_kib_vs_baseline']} KiB`
- `Slab`：`{baseline['slab_kib']} -> {trim1['slab_kib']}`，变化 `{trim1['delta_slab_kib_vs_baseline']} KiB`

这说明：

> 第一轮粗裁确实带来了镜像缩小，而且没有以牺牲启动链路、demo 模块、`function_graph` 为代价。

### 5.2 D18：更适合作为“当前终态”而不是“无 caveat 的横比项”

D18 classified 当前数据是：

- `Image = {trim2['image_kib']} KiB`
- `rootfs = {trim2['rootfs_kib']} KiB`
- `boot_ms = {trim2['boot_ms']}`
- `function_graph = {trim2['function_graph_ok']}`
- `perf = {trim2['perf_ok']}`

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
"""
    OUT_DRAFT.write_text(report, encoding='utf-8')


def write_final(rows: list[dict[str, str]]) -> None:
    baseline, trim1, trim2 = rows
    base_img = to_int(baseline, 'image_kib')
    trim1_img = to_int(trim1, 'image_kib')
    image_drop = base_img - trim1_img
    image_pct = abs((trim1_img - base_img) / base_img * 100) if base_img else 0.0
    base_boot = to_int(baseline, 'boot_ms')
    trim1_boot = to_int(trim1, 'boot_ms')
    base_mem = to_int(baseline, 'memfree_kib')
    trim1_mem = to_int(trim1, 'memfree_kib')
    base_slab = to_int(baseline, 'slab_kib')
    trim1_slab = to_int(trim1, 'slab_kib')
    report = f"""# Day19 对比报告（交付版）

## 1. 任务与结论

D19 的原始任务是：输出 `size / boot / mem / module 数` 对比，并列出风险项，形成一份对比报告草稿。

站在这个口径上，Day19 当前已经完成了第一版可交付收口。结论可以直接写成：

> **D15 baseline 到 D16 trim1(round1) 的第一轮粗裁收益已经有明确量化证据；D18 trim2(classified) 则证明了第二轮分类表达成立，并把 `perf` 带入了当前最终阶段。**

这句话里有两个层次：

1. **量化收益最稳的比较对象是 D15 vs D16。**
2. **D18 更适合作为“当前最终形态 + 方法旁证”，而不是不加说明地参与跨周期纯量化排名。**

---

## 2. 阶段映射与取数原则

Day19 对外阶段映射保持不变：

- `D15 = baseline`
- `D16 = trim1`
- `D18 = trim2`

内部取数则遵循“先复用已有沉淀”的原则：

- D15：取 `day15/RESULTS.md`
- D16：取 `day16/RESULTS_ROUND1.md`
- D18：取 `day18/records/compare-20260315-142441.csv`

这里的关键不是强行假装三阶段完全同口径，而是把**哪些能直接比较、哪些必须带 caveat** 说清楚。

---

## 3. 关键对比表

{render_table(rows)}

---

## 4. 核心解读

### 4.1 size：第一轮粗裁已经有明确收益

- D15 `Image = {baseline['image_kib']} KiB`
- D16 `Image = {trim1['image_kib']} KiB`
- 下降 `{image_drop} KiB`，约 `{image_pct:.1f}%`

这说明第一轮粗裁不是“配置看起来少了”，而是已经反映到启动镜像体积上。

### 4.2 boot：收益不大，但没有明显回退

- D15 `boot_ms = {baseline['boot_ms']}`
- D16 `boot_ms = {trim1['boot_ms']}`
- 变化 `{trim1['delta_boot_ms_vs_baseline']} ms`

这个量级更适合解读为“基本持平”。也就是说，当前裁剪带来了镜像缩小，但没有换来启动链路不稳。

### 4.3 mem：有轻微改善，方向正确

- D15 `MemFree = {baseline['memfree_kib']} KiB`，D16 为 `{trim1['memfree_kib']} KiB`
- D15 `Slab = {baseline['slab_kib']} KiB`，D16 为 `{trim1['slab_kib']} KiB`

当前可以保守地说：**内存侧变化不大，但方向是正向的。** 这与“先去掉无关驱动与子系统”的目标一致。

### 4.4 module 数：运行时模块数已覆盖，构建模块数需补齐

- `modules_loaded_count`：三阶段都能读到，当前都为 `1`
- `modules_built_count`：D18 明确为 `2`，D15/D16 在结果文档里未显式列出

因此 D19 的 `module 数` 维度已经能交付，但还不是完全对称的终态表达。这个缺口已经在风险矩阵里单独标出。

### 4.5 D18：结论成立，但要带 caveat

D18 trim2(classified) 当前可以确认：

- `function_graph = {trim2['function_graph_ok']}`
- `perf = {trim2['perf_ok']}`
- `pass_status = {trim2['pass_status']}`

同时 `classified` 与 `round2b_legacy` 的 `kernel.config sha256` 相同，说明 D18 的分类表达不是“只是换一种写法”，而是在**结果不变前提下，把配置组织方式整理得更清晰**。

但 D18 不能简单拿来和 D15 / D16 做“谁更优”的纯量化排名。原因是：

- rootfs 已进入带 perf 的新周期
- boot 行为也跟着新周期一起变化

所以 D18 在这版报告中的定位应该是：

> **当前最终阶段状态成立，且方法表达更清晰；但跨周期数字比较必须带说明。**

---

## 5. 风险矩阵摘要

本版报告最关键的风险不是“有没有表”，而是“表该怎么读”。当前需要明确的风险有：

1. **口径统一风险**：D15/D16 来自结果文档，D18 来自结构化 records。
2. **rootfs/perf 周期变化风险**：D18 的 rootfs 与 boot 已不处在 D15/D16 的同一周期内。
3. **平台扩展风险**：当前结论只在 `arm64 + QEMU virt + 当前 demo` 下成立。
4. **观测能力保留风险**：后续若只追求更小镜像，最容易先伤到 `ftrace/function_graph/perf`。
5. **module 数字段不完整风险**：D15/D16 缺少显式的 `modules_built_count`。

完整矩阵见：`output/day19_risk_matrix.md`

---

## 6. 验收判断

如果按 D19 原始验收口径来判断，这版 Day19 已经具备：

- 独立目录
- 数据来源说明
- 关键字段对比表
- 风险矩阵
- 报告草稿与交付版正文
- 生成脚本与结果产物

所以当前最合适的判断是：

> **Day19 已完成“对比报告草稿”的交付目标，并且已经具备继续增强为统一 records 版的工程基础。**

---

## 7. 下一步最值得做什么

后续如果只做一步增强，优先级最高的是：

> **补齐 D15 / D16 的结构化 records，再重新生成 Day19 报告。**

这样做的收益最大，因为它能直接把当前“草稿版口径”推进到“更硬的可复核口径”。
"""
    OUT_FINAL.write_text(report, encoding='utf-8')


def main() -> None:
    rows = load_rows()
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    write_csv(rows)
    write_draft(rows)
    write_final(rows)
    write_risk_matrix()
    write_summary(rows)
    write_acceptance(rows)
    print(f'generated: {OUT_CSV}')
    print(f'generated: {OUT_DRAFT}')
    print(f'generated: {OUT_FINAL}')
    print(f'generated: {OUT_RISK}')
    print(f'generated: {OUT_SUMMARY}')
    print(f'generated: {OUT_ACCEPT}')


if __name__ == '__main__':
    main()
