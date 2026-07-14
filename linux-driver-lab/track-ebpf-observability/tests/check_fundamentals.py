#!/usr/bin/env python3
"""审计 eBPF observability fundamentals 的完整性和入口一致性。"""

from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
FUNDAMENTALS = ROOT / "docs" / "fundamentals"
REQUIRED = [
    "README.md",
    "00_15_MINUTE_MENTAL_MODEL.md",
    "01_EBPF_KERNEL_ARCHITECTURE.md",
    "02_PROGRAM_TYPES_AND_HOOK_SELECTION.md",
    "03_VERIFIER_MEMORY_AND_SAFETY.md",
    "04_MAPS_STATE_AND_CONCURRENCY.md",
    "05_BPFTRACE_EXPLORATION_WORKFLOW.md",
    "06_KPROBE_FENTRY_AND_FUNCTION_TRACING.md",
    "07_TRACEPOINTS_AND_STABLE_EVENTS.md",
    "08_BTF_CORE_LIBBPF_AND_SKELETON.md",
    "09_RINGBUF_PERFBUF_AND_EVENT_TRANSPORT.md",
    "10_NETWORK_PATH_CORRELATION.md",
    "11_OVERHEAD_SAMPLING_AND_PRODUCTION.md",
    "12_DEBUGGING_PROJECT_MAP_AND_RECALL.md",
    "13_STACKS_SYMBOLIZATION_AND_FLAMEGRAPHS.md",
    "14_SECURITY_CAPABILITIES_AND_CONTAINERS.md",
]
LINK_RE = re.compile(r"\[[^\]]+\]\(([^)#]+)(?:#[^)]*)?\)")
MERMAID_RE = re.compile(r"^```mermaid\s*$", flags=re.MULTILINE)


def error(errors: list[str], message: str) -> None:
    errors.append(message)
    print(f"ERROR: {message}", file=sys.stderr)


def inspect(path: Path, errors: list[str]) -> tuple[int, int]:
    text = path.read_text(encoding="utf-8")
    lines = len(text.splitlines())
    if len(re.findall(r"^```", text, flags=re.MULTILINE)) % 2:
        error(errors, f"代码围栏未闭合: {path.relative_to(ROOT)}")
    for match in LINK_RE.finditer(text):
        target = match.group(1)
        if target.startswith(("http://", "https://", "mailto:", "/")):
            continue
        if not (path.parent / target).resolve().exists():
            error(errors, f"相对链接不存在: {path.relative_to(ROOT)} -> {target}")
    if path.name != "README.md" and lines < 65:
        error(errors, f"主题文档过短: {path.name} lines={lines} expected>=65")
    return lines, len(MERMAID_RE.findall(text))


def main() -> int:
    errors: list[str] = []
    paths: list[Path] = []
    for name in REQUIRED:
        path = FUNDAMENTALS / name
        if not path.exists():
            error(errors, f"缺少 eBPF fundamentals: {name}")
        else:
            paths.append(path)

    total_lines = 0
    mermaid = 0
    for path in paths:
        lines, diagrams = inspect(path, errors)
        total_lines += lines
        mermaid += diagrams
    if total_lines < 1400:
        error(errors, f"总篇幅不足: lines={total_lines} expected>=1400")
    if mermaid < 55:
        error(errors, f"Mermaid 图不足: actual={mermaid} expected>=55")

    marker = "EBPF_OBSERVABILITY_FUNDAMENTALS_COMPLETE"
    for name in ("README.md", "START_HERE.md", "ROADMAP.md"):
        text = (ROOT / name).read_text(encoding="utf-8")
        if "docs/fundamentals/README.md" not in text:
            error(errors, f"入口缺少 fundamentals 链接: {name}")
        if marker not in text:
            error(errors, f"入口缺少状态 marker: {name}")

    index = (FUNDAMENTALS / "README.md").read_text(encoding="utf-8")
    for name in REQUIRED[1:]:
        if name not in index:
            error(errors, f"fundamentals 索引未引用: {name}")

    if errors:
        print(f"EBPF_OBSERVABILITY_DOC_AUDIT_FAIL errors={len(errors)}")
        return 1
    print(
        "EBPF_OBSERVABILITY_DOC_AUDIT_PASS "
        f"files={len(paths)} lines={total_lines} mermaid={mermaid} links=pass"
    )
    print(marker)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

