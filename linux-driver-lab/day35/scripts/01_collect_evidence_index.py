#!/usr/bin/env python3
"""收集 day29~day34 的证据索引与关键指标。

这个脚本是 Day35 的核心：
1. 扫描前几天的 records 目录
2. 为每一天选出一个默认运行目录
3. 提取 run-summary 与常用 key=value 指标
4. 生成 evidence index 和 metrics CSV

Day35 不追求复杂数据库式索引，只追求“别人看到这份报告时，知道证据在哪、证明了什么”。
"""
from __future__ import annotations

import csv
import os
from pathlib import Path
from typing import Dict, List, Tuple

ROOT = Path(__file__).resolve().parents[2]
DAY35 = ROOT / "day35"
OUTPUT = DAY35 / "output"

DAYS = [29, 30, 31, 32, 33, 34]


def latest_run_dir(day: int) -> Path | None:
    records = ROOT / f"day{day}" / "records"
    if not records.exists():
        return None
    runs = sorted([p for p in records.iterdir() if p.is_dir() and not p.name.startswith(".")])
    return runs[-1] if runs else None


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="ignore") if path.exists() else ""


def parse_keyvals(text: str) -> Dict[str, str]:
    kv: Dict[str, str] = {}
    for raw in text.splitlines():
        line = raw.rstrip("\r\n")
        if "=" in line and not line.startswith("csv,"):
            key, value = line.split("=", 1)
            key = key.strip()
            value = value.strip()
            if key:
                kv[key] = value
        elif line.startswith("- ") and ": " in line:
            # run-summary.md 里是 markdown bullet，Day35 也会把它转成统一键值表。
            key, value = line[2:].split(": ", 1)
            kv[key.strip()] = value.strip()
    return kv


def add_metric(rows: List[List[str]], day: int, run_id: str, category: str, metric: str, value: str, source: str) -> None:
    rows.append([str(day), run_id, category, metric, value, source])


def main() -> None:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    evidence_lines: List[str] = ["# Day35 证据索引", ""]
    metrics_rows: List[List[str]] = [["day", "run_id", "category", "metric", "value", "source"]]

    for day in DAYS:
        run_dir = latest_run_dir(day)
        evidence_lines.append(f"## Day{day}")
        if run_dir is None:
            evidence_lines.extend(["", "未找到有效 records。", ""])
            continue

        run_id = run_dir.name
        evidence_lines.append("")
        evidence_lines.append(f"- run id: `{run_id}`")
        evidence_lines.append(f"- records path: `{run_dir.relative_to(ROOT)}`")

        summary = parse_keyvals(read_text(run_dir / "run-summary.md"))
        if summary:
            evidence_lines.append("- run-summary 摘要：")
            for key, value in summary.items():
                evidence_lines.append(f"  - {key}: {value}")
                if key in {
                    "verify ok",
                    "mmap verify ok",
                    "concurrent stress ok",
                    "module loop ok",
                    "fault invalid len ok",
                    "fault mmap offset ok",
                    "guest flow complete",
                    "qemu timeout hit",
                    "oops/dma-error/hung/panic found",
                    "avg latency gain pct",
                    "p99 latency gain pct",
                    "throughput gain pct",
                }:
                    add_metric(metrics_rows, day, run_id, "run-summary", key, value, str((run_dir / 'run-summary.md').relative_to(ROOT)))

        # Day29/30/33/34 主要看 verify / result / fault；Day31/32 主要看 bench。
        candidate_files = [
            "verify-result.txt",
            "mmap-verify.txt",
            "run-result.txt",
            "bench-ioctl.txt",
            "bench-mmap.txt",
            "bench-dma.txt",
            "bench-mmap-baseline.txt",
            "bench-mmap-optimized.txt",
            "compare-mmap.txt",
            "bench-dma-lite.txt",
            "concurrent-stress.txt",
            "module-loop.txt",
            "fault-invalid-len.txt",
            "fault-mmap-offset.txt",
            "trace-config.txt",
            "trace-window.txt",
        ]
        for fname in candidate_files:
            path = run_dir / fname
            if not path.exists():
                continue
            kv = parse_keyvals(read_text(path))
            if not kv:
                continue
            evidence_lines.append(f"- `{fname}` 关键字段：")
            for key, value in kv.items():
                # 只截取对 Day35 报告真正有意义的字段，避免索引文件过长。
                if key in {
                    "verify_ok",
                    "run_ok",
                    "run_error",
                    "irq_delta",
                    "mmap_ok",
                    "mmap_error",
                    "avg_us",
                    "p99_us",
                    "throughput_mbps",
                    "success_ops",
                    "failed_ops",
                    "worker_fail",
                    "worker_ioctl_rc",
                    "requested_loops",
                    "completed_loops",
                    "failed_loops",
                    "expected_failure",
                    "errno",
                    "avg_latency_gain_pct",
                    "p99_latency_gain_pct",
                    "throughput_gain_pct",
                    "trace_setup_failed",
                }:
                    evidence_lines.append(f"  - {key}={value}")
                    add_metric(metrics_rows, day, run_id, fname, key, value, str(path.relative_to(ROOT)))
        evidence_lines.append("")

    (OUTPUT / "day35_evidence_index.md").write_text("\n".join(evidence_lines) + "\n", encoding="utf-8")
    with (OUTPUT / "day35_metrics_summary.csv").open("w", encoding="utf-8", newline="") as fp:
        writer = csv.writer(fp)
        writer.writerows(metrics_rows)


if __name__ == "__main__":
    main()
