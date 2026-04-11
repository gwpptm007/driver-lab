#!/usr/bin/env python3
"""把教学用 DT 片段插入到 QEMU virt 导出的基础 DTS 中。"""

import argparse
from pathlib import Path


def build_parser() -> argparse.ArgumentParser:
    """构造命令行参数解析器。"""
    parser = argparse.ArgumentParser(
        description="Inject a demo DT fragment into QEMU virt DTS"
    )
    parser.add_argument("--input", required=True, help="Input DTS file")
    parser.add_argument("--fragment", required=True, help="Fragment file to insert")
    parser.add_argument("--output", required=True, help="Output DTS file")
    parser.add_argument(
        "--target-node",
        default="platform-bus@c000000",
        help="Target node name in base DTS. Default is QEMU virt platform bus",
    )
    return parser


def inject_fragment(base_text: str, fragment_text: str, target_node: str) -> str:
    """
    优先把片段插入到 platform-bus 节点内部。

    这样生成的教学节点会更接近真实的平台设备布局。
    如果目标节点没找到，就退化为插入到根节点末尾。
    """
    marker = f"\t{target_node} {{"

    if marker in base_text:
        insert_at = base_text.index(marker) + len(marker)
        indented = "\n\n\t\t" + fragment_text.replace("\n", "\n\t\t").rstrip("\t") + "\n"
        return base_text[:insert_at] + indented + base_text[insert_at:]

    fallback = "};\n"
    last_root_close = base_text.rfind(fallback)
    if last_root_close == -1:
        raise SystemExit("Could not find a suitable insertion point in base DTS")

    indented = "\n\t" + fragment_text.replace("\n", "\n\t").rstrip("\t") + "\n"
    return base_text[:last_root_close] + indented + base_text[last_root_close:]


def main() -> int:
    """脚本主入口。"""
    parser = build_parser()
    args = parser.parse_args()
    base_text = Path(args.input).read_text(encoding="utf-8")
    fragment_text = Path(args.fragment).read_text(encoding="utf-8").strip("\n") + "\n"
    merged = inject_fragment(base_text, fragment_text, args.target_node)
    Path(args.output).write_text(merged, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
