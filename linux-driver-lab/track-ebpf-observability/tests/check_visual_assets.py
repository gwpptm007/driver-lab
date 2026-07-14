#!/usr/bin/env python3
"""检查 eBPF 00-02 视觉化试点的 Canvas、PNG、GIF 与文档引用。"""

from __future__ import annotations

import struct
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
VISUALS = ROOT / "docs" / "fundamentals" / "visuals"
INTERACTIVE = VISUALS / "interactive"
ASSETS = VISUALS / "assets"

SCENES = {
    "00": {
        "html": "00_mental_model.html",
        "asset": "00_ebpf_event_journey",
        "doc": "00_15_MINUTE_MENTAL_MODEL.md",
        "anchors": ["sys_bpf", "bpf_check", "bpf_prog", "bpf_link", "ringbuf"],
    },
    "01": {
        "html": "01_kernel_lifecycle.html",
        "asset": "01_ebpf_object_lifecycle",
        "doc": "01_EBPF_KERNEL_ARCHITECTURE.md",
        "anchors": ["bpf_prog", "bpf_map", "bpf_link", "refcount", "bpffs"],
    },
    "02": {
        "html": "02_hook_selection.html",
        "asset": "02_ebpf_hook_journey",
        "doc": "02_PROGRAM_TYPES_AND_HOOK_SELECTION.md",
        "anchors": ["XDP", "TC", "tracepoint", "fentry", "kprobe"],
    },
}


def png_size(path: Path) -> tuple[int, int]:
    data = path.read_bytes()
    if not data.startswith(b"\x89PNG\r\n\x1a\n") or len(data) < 24:
        raise ValueError("PNG magic/structure invalid")
    return struct.unpack(">II", data[16:24])


def main() -> int:
    errors: list[str] = []

    common_files = [
        VISUALS / "README.md",
        INTERACTIVE / "visual-core.css",
        INTERACTIVE / "visual-core.js",
        VISUALS / "tools" / "render_visuals.ps1",
    ]
    for path in common_files:
        if not path.is_file():
            errors.append(f"missing common file: {path.relative_to(ROOT)}")

    # 资产目录只允许声明过的 PNG/GIF，防止失败渲染留下匿名或过期文件。
    expected_assets = {
        f'{spec["asset"]}.{suffix}'
        for spec in SCENES.values()
        for suffix in ("png", "gif")
    }
    actual_assets = {path.name for path in ASSETS.iterdir() if path.is_file()}
    unexpected_assets = sorted(actual_assets - expected_assets)
    if unexpected_assets:
        errors.append(f"unexpected visual assets: {', '.join(unexpected_assets)}")

    for scene_id, spec in SCENES.items():
        html_path = INTERACTIVE / spec["html"]
        png_path = ASSETS / f'{spec["asset"]}.png'
        gif_path = ASSETS / f'{spec["asset"]}.gif'
        doc_path = ROOT / "docs" / "fundamentals" / spec["doc"]

        if not html_path.is_file():
            errors.append(f"scene {scene_id}: missing HTML {html_path.relative_to(ROOT)}")
        else:
            html = html_path.read_text(encoding="utf-8")
            for token in [
                "<canvas",
                'data-action="play"',
                'data-action="pause"',
                'data-action="step"',
                'data-action="reset"',
                "EBPF_VISUAL_SCENE_READY",
            ]:
                if token not in html:
                    errors.append(f"scene {scene_id}: HTML missing token {token}")
            for anchor in spec["anchors"]:
                if anchor not in html:
                    errors.append(f"scene {scene_id}: missing source anchor {anchor}")

        if not png_path.is_file():
            errors.append(f"scene {scene_id}: missing PNG {png_path.relative_to(ROOT)}")
        else:
            try:
                width, height = png_size(png_path)
                if width < 900 or height < 500:
                    errors.append(f"scene {scene_id}: PNG too small {width}x{height}")
            except ValueError as exc:
                errors.append(f"scene {scene_id}: {exc}")

        if not gif_path.is_file():
            errors.append(f"scene {scene_id}: missing GIF {gif_path.relative_to(ROOT)}")
        else:
            data = gif_path.read_bytes()
            if not data.startswith((b"GIF87a", b"GIF89a")):
                errors.append(f"scene {scene_id}: GIF magic invalid")
            if len(data) < 20_000:
                errors.append(f"scene {scene_id}: GIF unexpectedly small bytes={len(data)}")

        if not doc_path.is_file():
            errors.append(f"scene {scene_id}: source document missing")
        else:
            doc = doc_path.read_text(encoding="utf-8")
            expected = [spec["html"], f'{spec["asset"]}.png', f'{spec["asset"]}.gif']
            for token in expected:
                if token not in doc:
                    errors.append(f"scene {scene_id}: document missing visual reference {token}")

    if errors:
        print(f"EBPF_VISUAL_ASSET_AUDIT_FAIL errors={len(errors)}")
        for error in errors:
            print(f"ERROR: {error}")
        return 1

    print(
        "EBPF_VISUAL_ASSET_AUDIT_PASS "
        f"scenes={len(SCENES)} png=3 gif=3 canvas=3"
    )
    print("EBPF_VISUAL_LEARNING_PILOT_COMPLETE")
    return 0


if __name__ == "__main__":
    sys.exit(main())
