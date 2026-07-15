#!/usr/bin/env python3
"""审计 track-real-driver 项目前知识层的结构、篇幅、图和相对链接。"""

from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DOC_DIR = ROOT / "docs" / "fundamentals"
TOPIC_NAMES = [f"{index:02d}_{name}.md" for index, name in enumerate([
    "15_MINUTE_MENTAL_MODEL",
    "DRIVER_MODEL_AND_KERNEL_POSITION",
    "BUS_MATCHING_PCI_AND_VIRTIO",
    "LIFECYCLE_PROBE_OPEN_STOP_REMOVE",
    "NET_DEVICE_PRIVATE_STATE_AND_OPS",
    "QUEUE_RING_DMA_AND_OWNERSHIP",
    "RX_IRQ_NAPI_AND_BUFFER_LIFECYCLE",
    "TX_XMIT_COMPLETION_AND_FLOW_CONTROL",
    "VIRTQUEUE_FEATURE_NEGOTIATION_AND_LAYOUT",
    "E1000E_HARDWARE_PATH_AND_INTERRUPTS",
    "OFFLOAD_ETHTOOL_STATS_AND_CONTROL_PLANE",
    "CONCURRENCY_LOCKING_AND_MEMORY_ORDERING",
    "SOURCE_READING_AND_CALL_GRAPH_WORKFLOW",
    "RUNTIME_OBSERVABILITY_AND_FAULT_LOCALIZATION",
    "PATCH_VALIDATION_UPSTREAM_AND_PROJECT_MAP",
])]
REQUIRED = [DOC_DIR / "README.md", *(DOC_DIR / name for name in TOPIC_NAMES)]
ENTRY_FILES = [ROOT / "README.md", ROOT / "START_HERE.md", ROOT / "ROADMAP.md"]
MARKER = "REAL_DRIVER_FUNDAMENTALS_COMPLETE"
LINK_RE = re.compile(r"(?<!!)\[[^]]+\]\(([^)]+)\)")


def check_links(path: Path, text: str, errors: list[str]) -> int:
    checked = 0
    for raw_target in LINK_RE.findall(text):
        target = raw_target.strip().split("#", 1)[0]
        if not target or "://" in target or target.startswith("#"):
            continue
        checked += 1
        candidate = (path.parent / target).resolve()
        if not candidate.exists():
            errors.append(f"相对链接不存在: {path.relative_to(ROOT)} -> {raw_target}")
    return checked


def main() -> int:
    errors: list[str] = []
    total_lines = 0
    mermaid_count = 0
    link_count = 0

    for path in [*REQUIRED, *ENTRY_FILES]:
        if not path.is_file():
            errors.append(f"缺少文件: {path.relative_to(ROOT)}")
            continue

        text = path.read_text(encoding="utf-8")
        lines = text.splitlines()
        if path in REQUIRED:
            total_lines += len(lines)
            mermaid_count += text.count("```mermaid")
        if path.name != "README.md" or path.parent == ROOT:
            if text.count("```") % 2:
                errors.append(f"Markdown 代码围栏未闭合: {path.relative_to(ROOT)}")
        link_count += check_links(path, text, errors)

    for path in REQUIRED[1:]:
        if path.is_file() and len(path.read_text(encoding="utf-8").splitlines()) < 70:
            errors.append(f"主题篇幅不足 70 行: {path.relative_to(ROOT)}")

    if total_lines < 1600:
        errors.append(f"知识层总篇幅不足 1600 行: actual={total_lines}")
    if mermaid_count < 90:
        errors.append(f"Mermaid 图不足 90 个: actual={mermaid_count}")

    for path in ENTRY_FILES:
        if path.is_file():
            text = path.read_text(encoding="utf-8")
            if MARKER not in text:
                errors.append(f"入口缺少 marker: {path.relative_to(ROOT)}")
            if "docs/fundamentals/README.md" not in text:
                errors.append(f"入口缺少 fundamentals 导航: {path.relative_to(ROOT)}")

    if errors:
        print(f"REAL_DRIVER_DOC_AUDIT_FAIL errors={len(errors)}")
        for error in errors:
            print(f"ERROR: {error}")
        return 1

    print(
        "REAL_DRIVER_DOC_AUDIT_PASS "
        f"files={len(REQUIRED)} lines={total_lines} "
        f"mermaid={mermaid_count} links={link_count}"
    )
    print(MARKER)
    return 0


if __name__ == "__main__":
    sys.exit(main())
