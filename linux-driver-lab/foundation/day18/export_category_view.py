#!/usr/bin/env python3
from __future__ import annotations

import csv
import pathlib
from collections import defaultdict

ROOT = pathlib.Path(__file__).resolve().parent
MANIFEST = ROOT / "config" / "category_manifest.csv"
OUT_MD = ROOT / "docs" / "02_category_matrix.md"

rows = list(csv.DictReader(MANIFEST.open(encoding="utf-8")))
groups: dict[str, list[dict[str, str]]] = defaultdict(list)
for row in rows:
    groups[row["category"]].append(row)

lines: list[str] = []
lines.append("# Day18 分类矩阵")
lines.append("")
lines.append("这份表把 day18 的分类裁剪拆成 **required / platform / debug / perf / trim** 五类。")
lines.append("它不是替代 `.config`，而是给学习、复盘、汇报时提供一张可解释的总览表。")
lines.append("")
for category in ["required", "platform", "debug", "perf", "trim"]:
    items = groups.get(category, [])
    if not items:
        continue
    lines.append(f"## {category}")
    lines.append("")
    lines.append("| symbol | expected | why_keep_or_trim | source_fragment |")
    lines.append("|---|---|---|---|")
    for item in items:
        lines.append(
            f"| {item['symbol']} | {item['expected']} | {item['why_keep_or_trim']} | {item['source_fragment']} |"
        )
    lines.append("")

OUT_MD.parent.mkdir(parents=True, exist_ok=True)
OUT_MD.write_text("\n".join(lines) + "\n", encoding="utf-8")
print(f"generated: {OUT_MD}")
