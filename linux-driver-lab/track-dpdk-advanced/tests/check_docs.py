#!/usr/bin/env python3
"""检查 Advanced 学习入口、链接、图和 Phase 7 状态一致性。"""

from __future__ import annotations

import re
import sys
from pathlib import Path


TRACK_ROOT = Path(__file__).resolve().parents[1]
FUNDAMENTALS = TRACK_ROOT / "docs" / "fundamentals"
REQUIRED = [
    "00_ADVANCED_MENTAL_MODEL.md",
    "01_HARDWARE_QUEUE_STEERING.md",
    "02_ADVANCED_MEMORY_AND_DATA_STRUCTURES.md",
    "03_MULTICORE_PIPELINE_DATA_PATH.md",
    "04_CONCURRENCY_RCU_QSBR.md",
    "05_PROJECT_KNOWLEDGE_MAP.md",
    "06_PROFILING_AND_DEBUGGING.md",
    "07_RECALL_CARDS.md",
]
LINK_RE = re.compile(r"\[[^\]]+\]\(([^)#]+)(?:#[^)]*)?\)")


def report_error(errors: list[str], message: str) -> None:
    errors.append(message)
    print(f"ERROR: {message}", file=sys.stderr)


def inspect_markdown(path: Path, errors: list[str]) -> int:
    text = path.read_text(encoding="utf-8")
    fences = len(re.findall(r"^```", text, flags=re.MULTILINE))
    if fences % 2:
        report_error(errors, f"代码围栏未闭合: {path.relative_to(TRACK_ROOT)}")

    for match in LINK_RE.finditer(text):
        target = match.group(1)
        if target.startswith(("http://", "https://", "mailto:", "/")):
            continue
        if not (path.parent / target).resolve().exists():
            report_error(
                errors,
                f"相对链接不存在: {path.relative_to(TRACK_ROOT)} -> {target}",
            )

    return len(re.findall(r"^```mermaid\s*$", text, flags=re.MULTILINE))


def main() -> int:
    errors: list[str] = []
    paths = [TRACK_ROOT / "README.md", TRACK_ROOT / "START_HERE.md"]
    for name in REQUIRED:
        path = FUNDAMENTALS / name
        if not path.exists():
            report_error(errors, f"缺少 Advanced fundamentals: {name}")
        else:
            paths.append(path)

    mermaid = sum(inspect_markdown(path, errors) for path in paths if path.exists())
    if mermaid < 25:
        report_error(errors, f"Mermaid 图数量不足: actual={mermaid} expected>=25")

    marker = "DPDK_FLOW_PIPELINE_CURRENT_ENV_COMPLETE"
    status_files = [
        TRACK_ROOT / "README.md",
        TRACK_ROOT / "ROADMAP.md",
        TRACK_ROOT / "docs" / "01_TRACK_OVERVIEW.md",
    ]
    for path in status_files:
        if marker not in path.read_text(encoding="utf-8"):
            report_error(errors, f"Phase 7 状态缺失: {path.relative_to(TRACK_ROOT)}")

    start_text = (TRACK_ROOT / "START_HERE.md").read_text(encoding="utf-8")
    if "track-dpdk/docs/fundamentals" not in start_text:
        report_error(errors, "START_HERE 缺少基础 track 前置知识入口")

    if errors:
        print(f"DPDK_ADVANCED_DOC_AUDIT_FAIL errors={len(errors)}")
        return 1

    print(
        "DPDK_ADVANCED_DOC_AUDIT_PASS "
        f"files={len(paths)} mermaid={mermaid} links=pass phase7=consistent"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
