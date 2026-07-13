#!/usr/bin/env python3
"""审计 AF_XDP fundamentals 的篇幅、图、链接和入口一致性。"""

from __future__ import annotations

import re
import sys
from pathlib import Path


TRACK_ROOT = Path(__file__).resolve().parents[1]
FUNDAMENTALS = TRACK_ROOT / "docs" / "fundamentals"
REQUIRED = [
    "README.md",
    "00_15_MINUTE_MENTAL_MODEL.md",
    "01_KERNEL_RX_AND_XDP_POSITION.md",
    "02_EBPF_VERIFIER_MAPS_AND_LOADER.md",
    "03_SOCKET_UMEM_AND_FRAME_LAYOUT.md",
    "04_FOUR_RINGS_AND_OWNERSHIP.md",
    "05_XSKMAP_REDIRECT_AND_QUEUE_BINDING.md",
    "06_COPY_ZEROCOPY_AND_DRIVER_DMA.md",
    "07_TX_REFLECT_AND_NEED_WAKEUP.md",
    "08_MULTIQUEUE_RSS_AND_SHARED_UMEM.md",
    "09_CONCURRENCY_AND_MEMORY_ORDER.md",
    "10_PERFORMANCE_NUMA_AND_MEASUREMENT.md",
    "11_DEBUGGING_PLAYBOOK.md",
    "12_PROJECT_MAP_AND_RECALL_CARDS.md",
]
LINK_RE = re.compile(r"\[[^\]]+\]\(([^)#]+)(?:#[^)]*)?\)")
MERMAID_RE = re.compile(r"^```mermaid\s*$", flags=re.MULTILINE)


def fail(errors: list[str], message: str) -> None:
    errors.append(message)
    print(f"ERROR: {message}", file=sys.stderr)


def inspect(path: Path, errors: list[str]) -> tuple[int, int]:
    text = path.read_text(encoding="utf-8")
    lines = len(text.splitlines())
    fences = len(re.findall(r"^```", text, flags=re.MULTILINE))
    if fences % 2:
        fail(errors, f"代码围栏未闭合: {path.relative_to(TRACK_ROOT)}")

    for match in LINK_RE.finditer(text):
        target = match.group(1)
        if target.startswith(("http://", "https://", "mailto:", "/")):
            continue
        if not (path.parent / target).resolve().exists():
            fail(errors, f"相对链接不存在: {path.relative_to(TRACK_ROOT)} -> {target}")

    # 主题文档必须能独立阅读，防止后续维护时退化成几行链接。
    if path.name != "README.md" and lines < 90:
        fail(errors, f"主题文档过短: {path.name} lines={lines} expected>=90")
    return lines, len(MERMAID_RE.findall(text))


def main() -> int:
    errors: list[str] = []
    paths: list[Path] = []
    for name in REQUIRED:
        path = FUNDAMENTALS / name
        if not path.exists():
            fail(errors, f"缺少 AF_XDP fundamentals: {name}")
        else:
            paths.append(path)

    total_lines = 0
    mermaid = 0
    for path in paths:
        lines, diagrams = inspect(path, errors)
        total_lines += lines
        mermaid += diagrams

    if total_lines < 1400:
        fail(errors, f"总篇幅不足: lines={total_lines} expected>=1400")
    if mermaid < 55:
        fail(errors, f"Mermaid 图不足: actual={mermaid} expected>=55")

    marker = "AF_XDP_FUNDAMENTALS_COMPLETE"
    for name in ("README.md", "START_HERE.md", "ROADMAP.md"):
        text = (TRACK_ROOT / name).read_text(encoding="utf-8")
        if "docs/fundamentals/README.md" not in text:
            fail(errors, f"入口缺少 fundamentals 链接: {name}")
        if marker not in text:
            fail(errors, f"入口缺少状态 marker: {name}")

    index = (FUNDAMENTALS / "README.md").read_text(encoding="utf-8")
    for name in REQUIRED[1:]:
        if name not in index:
            fail(errors, f"fundamentals 索引未引用: {name}")

    if errors:
        print(f"AF_XDP_FUNDAMENTALS_DOC_AUDIT_FAIL errors={len(errors)}")
        return 1

    print(
        "AF_XDP_FUNDAMENTALS_DOC_AUDIT_PASS "
        f"files={len(paths)} lines={total_lines} mermaid={mermaid} links=pass"
    )
    print(marker)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

