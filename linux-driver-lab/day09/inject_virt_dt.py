#!/usr/bin/env python3
import argparse
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description="Inject a demo DT fragment into QEMU virt DTS")
    parser.add_argument("--input", required=True, help="Input DTS file")
    parser.add_argument("--fragment", required=True, help="Fragment file to insert")
    parser.add_argument("--output", required=True, help="Output DTS file")
    parser.add_argument(
        "--target-node",
        default="platform-bus@c000000",
        help="Target node name in base DTS. Default is QEMU virt platform bus",
    )
    args = parser.parse_args()

    base_text = Path(args.input).read_text(encoding="utf-8")
    frag_text = Path(args.fragment).read_text(encoding="utf-8").strip("\n") + "\n"

    marker = f"\t{args.target_node} {{"
    if marker in base_text:
        insert_at = base_text.index(marker) + len(marker)
        injected = base_text[:insert_at] + "\n\n\t\t" + frag_text.replace("\n", "\n\t\t").rstrip("\t") + "\n" + base_text[insert_at:]
        Path(args.output).write_text(injected, encoding="utf-8")
        return 0

    fallback = "};\n"
    last_root_close = base_text.rfind(fallback)
    if last_root_close == -1:
        raise SystemExit("Could not find a suitable insertion point in base DTS")

    injected = base_text[:last_root_close] + "\n\t" + frag_text.replace("\n", "\n\t").rstrip("\t") + "\n" + base_text[last_root_close:]
    Path(args.output).write_text(injected, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
