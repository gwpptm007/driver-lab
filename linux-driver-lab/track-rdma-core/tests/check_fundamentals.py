#!/usr/bin/env python3
"""审计 RDMA fundamentals 的完整性、链接、图和入口状态。"""

from __future__ import annotations

import re
import sys
from pathlib import Path


TRACK_ROOT = Path(__file__).resolve().parents[1]
FUNDAMENTALS = TRACK_ROOT / "docs" / "fundamentals"
REQUIRED = [
    "README.md",
    "00_15_MINUTE_MENTAL_MODEL.md",
    "01_HARDWARE_KERNEL_USERSPACE_STACK.md",
    "02_VERBS_OBJECTS_LIFECYCLE.md",
    "03_MEMORY_REGISTRATION_DMA_KEYS.md",
    "04_QP_STATE_MACHINE_CONTROL_PLANE.md",
    "05_WR_WQE_CQE_DATA_PATH.md",
    "06_TRANSPORTS_ROCE_NETWORK.md",
    "07_ONE_SIDED_ATOMIC_CONSISTENCY.md",
    "08_PERFORMANCE_TUNING_NUMA.md",
    "09_RELIABILITY_SECURITY_FAILURES.md",
    "10_PROJECT_KNOWLEDGE_MAP.md",
    "11_DEBUGGING_PLAYBOOK.md",
    "12_RECALL_CARDS.md",
]
LINK_RE = re.compile(r"\[[^\]]+\]\(([^)#]+)(?:#[^)]*)?\)")
MERMAID_RE = re.compile(r"^```mermaid\s*$", flags=re.MULTILINE)


def report_error(errors: list[str], message: str) -> None:
    errors.append(message)
    print(f"ERROR: {message}", file=sys.stderr)


def inspect_markdown(path: Path, errors: list[str]) -> tuple[int, int]:
    text = path.read_text(encoding="utf-8")
    lines = len(text.splitlines())
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

    # 每个主题必须达到足以独立学习的篇幅，避免退化成只有链接的目录页。
    if path.name != "README.md" and lines < 90:
        report_error(errors, f"主题文档过短: {path.name} lines={lines} expected>=90")

    return lines, len(MERMAID_RE.findall(text))


def main() -> int:
    errors: list[str] = []
    paths: list[Path] = []
    for name in REQUIRED:
        path = FUNDAMENTALS / name
        if not path.exists():
            report_error(errors, f"缺少 RDMA fundamentals: {name}")
        else:
            paths.append(path)

    total_lines = 0
    mermaid = 0
    for path in paths:
        lines, diagrams = inspect_markdown(path, errors)
        total_lines += lines
        mermaid += diagrams

    if total_lines < 1500:
        report_error(errors, f"总篇幅不足: lines={total_lines} expected>=1500")
    if mermaid < 50:
        report_error(errors, f"Mermaid 图数量不足: actual={mermaid} expected>=50")

    marker = "RDMA_FUNDAMENTALS_COMPLETE"
    entry_files = [
        TRACK_ROOT / "README.md",
        TRACK_ROOT / "START_HERE.md",
        TRACK_ROOT / "ROADMAP.md",
    ]
    for path in entry_files:
        text = path.read_text(encoding="utf-8")
        if "docs/fundamentals/README.md" not in text:
            report_error(errors, f"入口缺少 fundamentals 链接: {path.name}")
        if marker not in text:
            report_error(errors, f"入口缺少状态 marker: {path.name}")

    index_text = (FUNDAMENTALS / "README.md").read_text(encoding="utf-8")
    for name in REQUIRED[1:]:
        if name not in index_text:
            report_error(errors, f"fundamentals 索引未引用: {name}")

    if errors:
        print(f"RDMA_FUNDAMENTALS_DOC_AUDIT_FAIL errors={len(errors)}")
        return 1

    print(
        "RDMA_FUNDAMENTALS_DOC_AUDIT_PASS "
        f"files={len(paths)} lines={total_lines} mermaid={mermaid} links=pass"
    )
    print(marker)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

