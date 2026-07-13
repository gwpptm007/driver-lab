#!/usr/bin/env python3
"""检查 track-dpdk 文档结构、链接、围栏和状态真源。"""

from __future__ import annotations

import re
import sys
from pathlib import Path


TRACK_ROOT = Path(__file__).resolve().parents[1]
FUNDAMENTALS = TRACK_ROOT / "docs" / "fundamentals"

REQUIRED_DOCS = [
    "00_10_MINUTE_MENTAL_MODEL.md",
    "01_KERNEL_AND_HARDWARE_POSITION.md",
    "02_CORE_OBJECTS_AND_MEMORY.md",
    "03_END_TO_END_DATA_PATH.md",
    "04_CONCURRENCY_AND_LIFECYCLE.md",
    "05_PROJECT_KNOWLEDGE_MAP.md",
    "06_DEBUGGING_PLAYBOOK.md",
    "07_RECALL_CARDS.md",
    "08_PACKET_FORMAT_AND_OFFLOADS.md",
    "09_ETHDEV_CAPABILITY_AND_PORT_CONFIG.md",
    "10_PERFORMANCE_AND_OBSERVABILITY.md",
    "11_VIRTIO_VHOST_DATA_PATH.md",
    "12_SAFE_ENVIRONMENT_PREPARATION.md",
]

LINK_RE = re.compile(r"\[[^\]]+\]\(([^)#]+)(?:#[^)]*)?\)")


def fail(errors: list[str], message: str) -> None:
    errors.append(message)
    print(f"ERROR: {message}", file=sys.stderr)


def check_file(path: Path, errors: list[str]) -> int:
    text = path.read_text(encoding="utf-8")
    fence_count = len(re.findall(r"^```", text, flags=re.MULTILINE))
    if fence_count % 2:
        fail(errors, f"代码围栏未闭合: {path.relative_to(TRACK_ROOT)}")

    for match in LINK_RE.finditer(text):
        target = match.group(1)
        if target.startswith(("http://", "https://", "mailto:", "/")):
            continue
        resolved = (path.parent / target).resolve()
        if not resolved.exists():
            fail(
                errors,
                f"相对链接不存在: {path.relative_to(TRACK_ROOT)} -> {target}",
            )

    return len(re.findall(r"^```mermaid\s*$", text, flags=re.MULTILINE))


def main() -> int:
    errors: list[str] = []
    paths = [TRACK_ROOT / "README.md", TRACK_ROOT / "START_HERE.md"]

    for name in REQUIRED_DOCS:
        path = FUNDAMENTALS / name
        if not path.exists():
            fail(errors, f"缺少基础文档: docs/fundamentals/{name}")
        else:
            paths.append(path)

    mermaid_count = sum(check_file(path, errors) for path in paths if path.exists())
    if mermaid_count < 40:
        fail(errors, f"Mermaid 图数量不足: actual={mermaid_count} expected>=40")

    status_text = (TRACK_ROOT / "docs" / "07_DPDK_TRACK_FINAL_STATUS.md").read_text(
        encoding="utf-8"
    )
    readme_text = (TRACK_ROOT / "README.md").read_text(encoding="utf-8")
    required_markers = [
        "PASS_PCAP_FUNCTIONAL",
        "PASS_PCAP_FORWARDING",
        "PASS_PCAP_REWRITE",
    ]
    for marker in required_markers:
        if marker not in status_text or marker not in readme_text:
            fail(errors, f"README 与状态真源未同时包含: {marker}")

    if "READY_TO_TEST" in readme_text:
        fail(errors, "README 仍包含已过时的 READY_TO_TEST")

    if errors:
        print(f"DPDK_TRACK_DOC_AUDIT_FAIL errors={len(errors)}")
        return 1

    print(
        "DPDK_TRACK_DOC_AUDIT_PASS "
        f"files={len(paths)} mermaid={mermaid_count} links=pass status=consistent"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
