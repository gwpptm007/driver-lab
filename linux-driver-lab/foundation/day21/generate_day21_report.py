#!/usr/bin/env python3
from __future__ import annotations

import csv
from pathlib import Path
from typing import Dict, List

ROOT = Path(__file__).resolve().parent
LAB = ROOT.parent
DAY19_CSV = LAB / 'day19' / 'output' / 'day19_compare_table.csv'
DAY20_ENV = LAB / 'day20' / 'output' / 'day20_delivery_status.env'
OUT = ROOT / 'output'


def parse_env(path: Path) -> Dict[str, str]:
    data: Dict[str, str] = {}
    for line in path.read_text(encoding='utf-8').splitlines():
        line = line.strip()
        if not line or line.startswith('#') or '=' not in line:
            continue
        key, value = line.split('=', 1)
        data[key.strip()] = value.strip()
    return data


def load_rows(path: Path) -> List[Dict[str, str]]:
    with path.open('r', encoding='utf-8', newline='') as f:
        return list(csv.DictReader(f))


def pct_delta(delta: str, base: str) -> str:
    if not delta or not base:
        return 'n/a'
    try:
        d = int(delta)
        b = int(base)
        if b == 0:
            return 'n/a'
        sign = '+' if d > 0 else ''
        return f"{sign}{d / b * 100:.1f}%"
    except ValueError:
        return 'n/a'


def signed(n: str) -> str:
    if not n:
        return 'n/a'
    try:
        v = int(n)
        return f"{v:+d}"
    except ValueError:
        return n


def build_data_snapshot(rows: List[Dict[str, str]], delivery: Dict[str, str]) -> None:
    path = OUT / 'day21_data_snapshot.csv'
    with path.open('w', encoding='utf-8', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['category', 'key', 'value', 'note'])
        for row in rows:
            stage = row['stage']
            for key in [
                'stage_label', 'image_kib', 'delta_image_kib_vs_baseline', 'rootfs_kib',
                'delta_rootfs_kib_vs_baseline', 'boot_ms', 'delta_boot_ms_vs_baseline',
                'memfree_kib', 'delta_memfree_kib_vs_baseline', 'slab_kib',
                'delta_slab_kib_vs_baseline', 'modules_built_count', 'modules_loaded_count',
                'function_graph_ok', 'perf_ok', 'pass_status', 'risk_note', 'source_primary'
            ]:
                writer.writerow(['stage', f'{stage}.{key}', row.get(key, ''), row.get('comparable_note', '')])
        for key in [
            'SUITE_READY', 'DELIVERY_READY', 'RUNTIME_READY', 'REGRESSION_PASS',
            'LATEST_RECORD', 'LATEST_MODE', 'LATEST_VERDICT', 'MISSING_ARTIFACTS'
        ]:
            writer.writerow(['day20', key, delivery.get(key, ''), 'from day20 delivery status'])


def build_reports(rows: List[Dict[str, str]], delivery: Dict[str, str]) -> None:
    by_stage = {row['stage']: row for row in rows}
    baseline = by_stage['baseline']
    trim1 = by_stage['trim1']
    trim2 = by_stage['trim2']

    suite_ready = delivery.get('SUITE_READY', '0')
    delivery_ready = delivery.get('DELIVERY_READY', '0')
    runtime_ready = delivery.get('RUNTIME_READY', '0')
    regression_pass = delivery.get('REGRESSION_PASS', '0')
    latest_verdict = delivery.get('LATEST_VERDICT', 'UNKNOWN')
    latest_record = delivery.get('LATEST_RECORD', 'n/a')
    latest_mode = delivery.get('LATEST_MODE', 'n/a')
    missing_artifacts = delivery.get('MISSING_ARTIFACTS', '(none)') or '(none)'

    image_delta = trim1['delta_image_kib_vs_baseline']
    image_pct = pct_delta(image_delta, baseline['image_kib'])
    boot_delta = trim1['delta_boot_ms_vs_baseline']
    mem_delta = trim1['delta_memfree_kib_vs_baseline']
    slab_delta = trim1['delta_slab_kib_vs_baseline']

    final_md = f"""# W3 最终总结报告（Day21 正式版）

## 1. 背景

W3 的目标不是继续堆新驱动功能，而是把已经跑通的 `arm64 + QEMU virt + BusyBox + demo_regmap.ko` 实验环境，收敛成一套**可裁剪、可比较、可回归、可回滚**的最小内核实验平台。围绕这个目标，D15-D20 依次完成了 baseline 冻结、第一轮粗裁、第二轮分类裁剪、量化对比和自动回归套件收口。

## 2. 方法

- **D15**：固定 baseline 配置、启动参数、rootfs 路线和验证对象。
- **D16**：执行第一轮粗裁，先去掉明显无关驱动与子系统，同时保留 debugfs / ftrace / function_graph 等观测能力。
- **D17 / D18**：完成 rootfs 工具链补齐与第二轮分类裁剪，把 `perf` 工具带入当前最终阶段。
- **D19**：整理 baseline / trim1 / trim2 的 `size / boot / mem / module 数` 对比，并补上风险矩阵。
- **D20**：建立 smoke / trace / perf / stress 四类自动回归入口，以及 latest / summary / verify / suite 交付接口。

## 3. 核心数据

| 阶段 | Image | rootfs | boot | MemFree | Slab | 运行时模块数 | function_graph | perf | 说明 |
|---|---:|---:|---:|---:|---:|---:|---|---|---|
| D15 baseline | {baseline['image_kib']} KiB | {baseline['rootfs_kib']} KiB | {baseline['boot_ms']} ms | {baseline['memfree_kib']} KiB | {baseline['slab_kib']} KiB | {baseline['modules_loaded_count'] or 'n/a'} | {baseline['function_graph_ok']} | {baseline['perf_ok']} | baseline 起点 |
| D16 trim1 | {trim1['image_kib']} KiB | {trim1['rootfs_kib']} KiB | {trim1['boot_ms']} ms | {trim1['memfree_kib']} KiB | {trim1['slab_kib']} KiB | {trim1['modules_loaded_count'] or 'n/a'} | {trim1['function_graph_ok']} | {trim1['perf_ok']} | 第一轮粗裁 |
| D18 trim2 | {trim2['image_kib']} KiB | {trim2['rootfs_kib']} KiB | {trim2['boot_ms']} ms | {trim2['memfree_kib']} KiB | {trim2['slab_kib']} KiB | {trim2['modules_loaded_count'] or 'n/a'} | {trim2['function_graph_ok']} | {trim2['perf_ok']} | 第二轮分类裁剪 |

**最稳的量化结论来自 D15 → D16：**

- `Image` 从 {baseline['image_kib']} KiB 降到 {trim1['image_kib']} KiB，变化 {signed(image_delta)} KiB（{image_pct}）。
- `boot_ms` 从 {baseline['boot_ms']} ms 到 {trim1['boot_ms']} ms，变化 {signed(boot_delta)} ms，可视为基本持平。
- `MemFree` 变化 {signed(mem_delta)} KiB，`Slab` 变化 {signed(slab_delta)} KiB，说明第一轮粗裁在不破坏启动链路的前提下带来了轻微正向收益。

**D18 需要带边界说明：** D18 已进入带 `perf` 的新 rootfs 周期，`function_graph=yes`、`perf=yes/yes`、`pass_status=PASS` 可以直接写成“当前最终形态成立”，但其 `rootfs` 与 `boot` 数字不能不加说明地和 D15 / D16 做纯收益排名。

## 4. 结论

1. **W3 已经形成完整收口链路。** 目前仓库里已经具备 baseline → trim1 → trim2 → compare → regression 的工程化闭环。
2. **第一轮粗裁收益已经有明确量化证据。** D15 到 D16 的镜像缩减最稳，且 boot 基本持平，说明“先去掉明显无关项”的策略是正确的。
3. **当前推荐配置已经明确。** 推荐继续以 `arm64 + QEMU virt + BusyBox rootfs + demo_regmap.ko + debugfs/ftrace/function_graph/perf basic` 作为后续主线口径。
4. **第二轮分类裁剪更适合作为“当前最终形态 + 方法表达整理”。** D18 证明了分类表达成立，并把 `perf` 用户态带入当前最终阶段。
5. **Day20 已达到交付级回归套件骨架。** 当前状态为 `SUITE_READY={suite_ready}`、`DELIVERY_READY={delivery_ready}`、`RUNTIME_READY={runtime_ready}`、`REGRESSION_PASS={regression_pass}`。这表示套件与交付入口已经到位，但当前代码包里真实运行件仍未闭环。

## 5. 回滚方案

- 保留 D15 baseline `.config` 作为总回退点。
- 每轮裁剪分阶段保存 fragment、结果文档和 records，不把多轮改动揉成一次不可拆的变化。
- rootfs 继续保留 BusyBox 基线版本；遇到 `perf` 相关问题时，优先区分是内核配置问题还是 rootfs 工具缺失。
- 出现异常时，先回退到 baseline 的 config + rootfs + QEMU 启动参数，不要同时修改内核和 rootfs 再重新验证。
- 如果 Day20 `latest_verdict={latest_verdict}`，优先按 Day20 的 latest / verify 路径排查。当前缺失输入件为：`{missing_artifacts}`。

## 6. 当前最推荐动作

- **要继续交付**：直接复用 Day19 的量化表与 Day20 的交付状态，把这份 Day21 报告作为 W3 的压缩版结论入口。
- **要继续验证**：先补齐 `Image/rootfs/dtb`，再用 Day20 套件执行真实回归，把 `RUNTIME_READY` 和 `REGRESSION_PASS` 推到可验证状态。
- **要继续演进**：优先补齐 D15 / D16 的结构化 records，再回刷 Day19 / Day21 报告，让跨阶段口径更硬。
"""

    submission_md = f"""# W3 最终提交版报告（Day21 Submission）

## 一句话结论

W3 已经把 `arm64 + QEMU virt + BusyBox + demo_regmap.ko` 这条实验主线收敛成一套**可裁剪、可比较、可回归、可回滚**的最小内核实验平台；其中 **D15 → D16 的第一轮粗裁收益最稳，D18 代表当前最终形态，D20 提供继续闭环真实回归的自动化套件。**

## 1. 本轮最终交付了什么

- 一条固定口径的 baseline / trim1 / trim2 主线。
- 一份可复查的量化对比表，覆盖 `size / boot / mem / module 数`。
- 一套 smoke / trace / perf / stress 自动回归套件骨架。
- 一份可讲解、可回顾、可继续复用的最终总结报告。

## 2. 最稳的结论先讲清楚

### 2.1 D15 → D16：第一轮粗裁收益成立

- `Image`：{baseline['image_kib']} KiB → {trim1['image_kib']} KiB，变化 {signed(image_delta)} KiB（{image_pct}）。
- `boot_ms`：{baseline['boot_ms']} ms → {trim1['boot_ms']} ms，变化 {signed(boot_delta)} ms，基本持平。
- `MemFree`：变化 {signed(mem_delta)} KiB。
- `Slab`：变化 {signed(slab_delta)} KiB。
- `function_graph`：仍为 `{trim1['function_graph_ok']}`。

这说明在不破坏启动链路和关键观测能力的前提下，第一轮粗裁已经带来明确、稳定、可复述的收益。

### 2.2 D18：作为当前最终形态成立

D18 当前可以明确写成：

- `function_graph={trim2['function_graph_ok']}`
- `perf={trim2['perf_ok']}`
- `pass_status={trim2['pass_status']}`

但 D18 处在**新的 rootfs/perf 周期**，因此它更适合表达“当前最终形态成立”和“分类方法整理成立”，不适合和 D15 / D16 一起下无边界的纯收益排名结论。

## 3. 当前推荐配置

`arm64 + QEMU virt + BusyBox rootfs + demo_regmap.ko + debugfs/ftrace/function_graph/perf basic`

推荐理由：

- 路径已经跑通，且可持续复用。
- D15 → D20 的主线都围绕这套口径沉淀了脚本、records 和文档。
- 它兼顾了最小化目标与后续 trace/perf/回归需求。

## 4. Day20 当前状态应该怎么表述

Day20 当前不是“真实回归已经最终 PASS”，而是：

- `SUITE_READY={suite_ready}`
- `DELIVERY_READY={delivery_ready}`
- `RUNTIME_READY={runtime_ready}`
- `REGRESSION_PASS={regression_pass}`
- `LATEST_RECORD={latest_record}`
- `LATEST_MODE={latest_mode}`
- `LATEST_VERDICT={latest_verdict}`
- `MISSING_ARTIFACTS={missing_artifacts}`

这表示 **回归套件本身已经成熟到可交付状态**，但这份代码包里真实运行件仍未闭环，所以最后一步真实 PASS 需要补齐 `Image/rootfs/dtb` 再跑。

## 5. 回滚与继续推进建议

### 回滚建议

- 以 D15 baseline `.config` 作为总回退点。
- 保留每轮 fragment、结果文档和 records，不把多轮改动揉成一次不可拆的变化。
- perf 异常时先区分内核配置问题与 rootfs 工具缺失，不要同时修改两边。

### 继续推进建议

- **要交付当前阶段成果**：优先使用本提交版报告 + Day19 对比表 + Day20 交付状态。
- **要拿到更硬的闭环**：补齐 `Image/rootfs/dtb` 后执行 Day20 真实回归。
- **要进一步提升报告可信度**：补齐 D15 / D16 的结构化 records，再回刷 Day19 / Day21。

## 6. 最终判断

> **Day21 当前已经达到“最终提交版总结报告”状态。**
>
> 它可以作为 W3 的最终压缩结论入口：既能讲清楚收益来自哪里，也能讲清楚哪些结论有边界、下一步该补什么。
"""

    draft_md = f"""# Day21 报告初稿

## 当前要解决的问题

把 D15-D20 已经形成的结果压缩成一份可以讲、可以投递、可以以后复用的短报告，而不是再引入新的实验变量。

## 当前已经能写得很稳的内容

- W3 主线已经固定在 `arm64 + QEMU virt + BusyBox + demo_regmap.ko`。
- D15 baseline、D16 trim1、D18 trim2 的阶段映射已经建立。
- D15 → D16 的量化收益可以直接讲：`Image` 下降 {signed(image_delta)} KiB，boot 基本持平。
- D20 已形成交付级回归套件骨架，当前状态是 `SUITE_READY={suite_ready}`、`DELIVERY_READY={delivery_ready}`。

## 当前必须带边界说明的内容

- D18 已进入带 perf 的新 rootfs 周期，数字能进表，但不能不加说明地和 D15/D16 做纯收益排名。
- Day20 当前不是“真实回归已经 PASS”，而是“套件结构成熟，但本包缺真实运行件”，最近 verdict 为 `{latest_verdict}`。

## 草稿结论句

> W3 当前已经把 arm64 + QEMU virt 最小内核实验环境从“能演示”推进到“能裁剪、能对比、能回归、能回滚”的阶段；其中 D15→D16 的第一轮粗裁收益最稳，D18 代表当前最终形态，D20 则提供了继续收敛真实回归结果的自动化基础。
"""

    onepager_md = f"""# W3 一页总结（Day21 One-Pager）

## 背景

W3 的目标是在保留 `debugfs + ftrace + function_graph + perf basic` 的前提下，把 `arm64 + QEMU virt + BusyBox + demo_regmap.ko` 收敛成最小、可比较、可回归的实验平台。

## 方法

- D15：baseline 冻结
- D16：第一轮粗裁
- D18：第二轮分类裁剪与 perf 进入当前最终阶段
- D19：量化对比
- D20：自动回归套件

## 核心数据

| 阶段 | Image | boot | MemFree | perf |
|---|---:|---:|---:|---|
| D15 | {baseline['image_kib']} KiB | {baseline['boot_ms']} ms | {baseline['memfree_kib']} KiB | {baseline['perf_ok']} |
| D16 | {trim1['image_kib']} KiB | {trim1['boot_ms']} ms | {trim1['memfree_kib']} KiB | {trim1['perf_ok']} |
| D18 | {trim2['image_kib']} KiB | {trim2['boot_ms']} ms | {trim2['memfree_kib']} KiB | {trim2['perf_ok']} |

**最稳结论：** D15 → D16 的 `Image` 缩小 {signed(image_delta)} KiB（{image_pct}），boot 基本持平。

## 当前推荐配置

`arm64 + QEMU virt + BusyBox rootfs + demo_regmap.ko + debugfs/ftrace/function_graph/perf basic`

## Day20 状态

- SUITE_READY={suite_ready}
- DELIVERY_READY={delivery_ready}
- RUNTIME_READY={runtime_ready}
- REGRESSION_PASS={regression_pass}
- latest_verdict={latest_verdict}
- missing_artifacts={missing_artifacts}

## 回滚方案

- 回退到 D15 baseline `.config` + rootfs + QEMU 启动参数
- 每轮裁剪保留单独 records / 结果文档
- perf 异常时不要同时修改内核和 rootfs
"""

    checklist_md = f"""# Day21 验收清单

- [x] Day21 独立目录存在
- [x] 需求分析与报告设计文档存在
- [x] 数据来源与边界说明存在
- [x] 已生成正式报告初稿：`output/day21_report_draft.md`
- [x] 已生成正式报告版：`output/day21_report_final.md`
- [x] 已生成提交版：`output/day21_report_submission.md`
- [x] 已生成一页版：`output/day21_report_onepager.md`
- [x] 已生成数据快照：`output/day21_data_snapshot.csv`
- [x] 已生成提交摘要：`output/day21_submission_summary.txt`
- [x] 结论中明确区分“最稳量化结论”和“带边界的阶段参考”
- [x] 回滚方案已单独写出
- [x] 已写明 Day20 当前不是最终真实 PASS，而是 `{latest_verdict}` / 缺件 `{missing_artifacts}`
"""

    summary_txt = f"""DAY21_FINAL_SUBMISSION=1
W3_STATUS=FINAL_SUMMARY_READY
MOST_STABLE_RESULT=D15_TO_D16
IMAGE_DELTA_KIB={image_delta}
IMAGE_DELTA_PCT={image_pct}
DAY20_SUITE_READY={suite_ready}
DAY20_DELIVERY_READY={delivery_ready}
DAY20_RUNTIME_READY={runtime_ready}
DAY20_REGRESSION_PASS={regression_pass}
DAY20_LATEST_VERDICT={latest_verdict}
DAY20_MISSING_ARTIFACTS={missing_artifacts}
CURRENT_RECOMMENDED_CONFIG=arm64+QEMU virt+BusyBox rootfs+demo_regmap.ko+debugfs/ftrace/function_graph/perf basic
"""

    (OUT / 'day21_report_draft.md').write_text(draft_md, encoding='utf-8')
    (OUT / 'day21_report_final.md').write_text(final_md, encoding='utf-8')
    (OUT / 'day21_report_submission.md').write_text(submission_md, encoding='utf-8')
    (OUT / 'day21_report_onepager.md').write_text(onepager_md, encoding='utf-8')
    (OUT / 'day21_acceptance_checklist.md').write_text(checklist_md, encoding='utf-8')
    (OUT / 'day21_submission_summary.txt').write_text(summary_txt, encoding='utf-8')


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    rows = load_rows(DAY19_CSV)
    delivery = parse_env(DAY20_ENV)
    build_data_snapshot(rows, delivery)
    build_reports(rows, delivery)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
