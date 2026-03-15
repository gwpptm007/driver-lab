#!/usr/bin/env python3
"""Day18 profile compare result aggregator with evidence-chain outputs."""
from __future__ import annotations

import argparse
import csv
import datetime as dt
import difflib
import hashlib
import pathlib
from typing import Dict, List, Optional, Tuple

ROOT = pathlib.Path(__file__).resolve().parent
RECORDS_ROOT = ROOT / "records"
DEFAULT_PROFILES = ["baseline", "round2b_legacy", "classified"]

FIELDS = [
    "profile",
    "scenario_id",
    "record_dir",
    "collector_ver",
    "boot_ms",
    "image_kib",
    "rootfs_kib",
    "modules_built_count",
    "modules_loaded_count",
    "memtotal_kib",
    "memfree_kib",
    "memavailable_kib",
    "slab_kib",
    "sreclaimable_kib",
    "sunreclaim_kib",
    "kernelstack_kib",
    "pagetables_kib",
    "debugfs_ok",
    "tracing_ok",
    "function_graph_ok",
    "trace_smoke_ok",
    "perf_bin_ok",
    "perf_smoke_ok",
    "boot_ok",
    "insmod_ok",
    "snapshot_ok",
    "trigger_ok",
    "dmesg_warn",
    "remarks",
    "kernel_release",
    "kernel_config_sha256",
    "kernel_config_line_count",
    "kernel_savedefconfig_sha256",
    "kernel_savedefconfig_line_count",
    "kernel_image_sha256",
    "kernel_image_bytes",
    "rootfs_img_sha256",
    "rootfs_img_bytes",
    "module_demo_sha256",
    "module_demo_bytes",
    "perf_manifest_present",
    "category_manifest_present",
    "config_diff_vs_baseline",
    "image_hash_vs_baseline",
    "rootfs_hash_vs_baseline",
]

PASS_KEYS = [
    "boot_ok",
    "debugfs_ok",
    "tracing_ok",
    "function_graph_ok",
    "trace_smoke_ok",
    "insmod_ok",
    "snapshot_ok",
    "trigger_ok",
]


def parse_env(path: pathlib.Path) -> Dict[str, str]:
    data: Dict[str, str] = {}
    for raw in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw.strip()
        if not line or "=" not in line:
            continue
        k, v = line.split("=", 1)
        v = v.strip()
        if len(v) >= 2 and v[0] == v[-1] and v[0] in ("'", '"'):
            v = v[1:-1]
        data[k] = v
    return data


def latest_record_for_profile(records_root: pathlib.Path, profile: str) -> Optional[pathlib.Path]:
    ptr = records_root / f"LAST_{profile}.txt"
    if ptr.exists():
        target = pathlib.Path(ptr.read_text(encoding="utf-8").strip())
        if target.exists():
            return target
    candidates = sorted(records_root.glob(f"*-day18-{profile}-arm64-virt"))
    return candidates[-1] if candidates else None


def as_int(value: str) -> Optional[int]:
    try:
        return int(value)
    except Exception:
        return None


def derive_pass_status(env: Dict[str, str]) -> str:
    for key in PASS_KEYS:
        if env.get(key) != "yes":
            return "FAIL"
    if env.get("dmesg_warn") == "yes":
        return "WARN"
    if env.get("perf_bin_ok") != "yes":
        return "WARN"
    if env.get("perf_smoke_ok") != "yes":
        return "WARN"
    return "PASS"


def delta_str(baseline: Dict[str, str], row: Dict[str, str], key: str, smaller_is_better: bool) -> str:
    b = as_int(baseline.get(key, ""))
    v = as_int(row.get(key, ""))
    if b is None or v is None:
        return "n/a"
    delta = v - b
    sign = "+" if delta > 0 else ""
    hint = "smaller is better" if smaller_is_better else "larger is better"
    return f"{sign}{delta} ({hint})"


def sha256_text(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def load_evidence(record_dir: pathlib.Path) -> Dict[str, str]:
    evidence = record_dir / "build_evidence" / "artifact_evidence.env"
    if evidence.exists():
        return parse_env(evidence)
    return {}


def read_lines(path: pathlib.Path) -> List[str]:
    if not path.exists():
        return []
    return path.read_text(encoding="utf-8", errors="replace").splitlines()


def config_diff_summary(a: pathlib.Path, b: pathlib.Path) -> Tuple[str, List[str]]:
    a_lines = read_lines(a)
    b_lines = read_lines(b)
    if not a_lines or not b_lines:
        return "n/a", []
    diff = list(difflib.unified_diff(a_lines, b_lines, fromfile=str(a), tofile=str(b), lineterm=""))
    changed = sum(1 for line in diff if line.startswith(("+CONFIG_", "+# CONFIG_", "-CONFIG_", "-# CONFIG_")))
    a_sha = sha256_text("\n".join(a_lines))
    b_sha = sha256_text("\n".join(b_lines))
    sha_equal = "yes" if a_sha == b_sha else "no"
    summary = f"changed_config_lines={changed} sha_equal={sha_equal}"
    return summary, diff


def write_diff(path: pathlib.Path, diff_lines: List[str], title: str) -> None:
    with path.open("w", encoding="utf-8") as f:
        f.write(f"# {title}\n\n")
        if diff_lines:
            f.write("\n".join(diff_lines) + "\n")
        else:
            f.write("# no diff or source file missing\n")


def build_rows(records_root: pathlib.Path, profiles: List[str]) -> List[Dict[str, str]]:
    rows: List[Dict[str, str]] = []
    for profile in profiles:
        record_dir = latest_record_for_profile(records_root, profile)
        if record_dir is None:
            raise FileNotFoundError(f"latest record for profile {profile} not found under {records_root}")
        env_path = record_dir / "metrics.env"
        if not env_path.exists():
            raise FileNotFoundError(f"metrics.env not found: {env_path}")
        env = parse_env(env_path)
        ev = load_evidence(record_dir)
        row = {field: env.get(field, ev.get(field, "")) for field in FIELDS}
        for k, v in ev.items():
            row[k] = v
        row["profile"] = profile
        row["record_dir"] = str(record_dir)
        row["pass_status"] = derive_pass_status(env)
        rows.append(row)
    return rows


def write_csv(path: pathlib.Path, rows: List[Dict[str, str]]) -> None:
    header = FIELDS + ["pass_status"]
    with path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=header)
        writer.writeheader()
        for row in rows:
            writer.writerow({k: row.get(k, "") for k in header})


def write_md(path: pathlib.Path, rows: List[Dict[str, str]], evidence_files: Dict[str, pathlib.Path], diff_summaries: Dict[str, str]) -> None:
    baseline = next((r for r in rows if r["profile"] == "baseline"), rows[0])
    lines: List[str] = []
    now = dt.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    lines.append(f"# Day18 profile compare summary ({now})")
    lines.append("")
    lines.append("## 1. 对比结论")
    lines.append("")
    for row in rows:
        lines.append(
            f"- **{row['profile']}**: status={row['pass_status']}, boot_ms={row.get('boot_ms','')}, "
            f"image_kib={row.get('image_kib','')}, rootfs_kib={row.get('rootfs_kib','')}, "
            f"perf={row.get('perf_bin_ok','')}/{row.get('perf_smoke_ok','')}, remarks={row.get('remarks','')}"
        )
    lines.append("")
    lines.append("## 2. 关键指标表")
    lines.append("")
    lines.append("| profile | status | boot_ms | Δboot_ms vs baseline | image_kib | Δimage_kib | rootfs_kib | Δrootfs_kib | memfree_kib | Δmemfree_kib | perf_bin_ok | perf_smoke_ok | remarks |")
    lines.append("|---|---|---:|---|---:|---|---:|---|---:|---|---|---|---|")
    for row in rows:
        lines.append(
            f"| {row['profile']} | {row['pass_status']} | {row.get('boot_ms','')} | {delta_str(baseline, row, 'boot_ms', True)} | "
            f"{row.get('image_kib','')} | {delta_str(baseline, row, 'image_kib', True)} | "
            f"{row.get('rootfs_kib','')} | {delta_str(baseline, row, 'rootfs_kib', True)} | "
            f"{row.get('memfree_kib','')} | {delta_str(baseline, row, 'memfree_kib', False)} | "
            f"{row.get('perf_bin_ok','')} | {row.get('perf_smoke_ok','')} | {row.get('remarks','')} |"
        )
    lines.append("")
    lines.append("## 3. 证据链表")
    lines.append("")
    lines.append("| profile | kernel_release | kernel_config_sha256 | kernel_savedefconfig_sha256 | kernel_image_sha256 | rootfs_img_sha256 | config_diff_vs_baseline | image_hash_vs_baseline | rootfs_hash_vs_baseline | category_manifest_present |")
    lines.append("|---|---|---|---|---|---|---|---|---|---|")
    for row in rows:
        lines.append(
            f"| {row['profile']} | {row.get('kernel_release','')} | {row.get('kernel_config_sha256','')} | "
            f"{row.get('kernel_savedefconfig_sha256','')} | {row.get('kernel_image_sha256','')} | {row.get('rootfs_img_sha256','')} | "
            f"{row.get('config_diff_vs_baseline','')} | {row.get('image_hash_vs_baseline','')} | {row.get('rootfs_hash_vs_baseline','')} | "
            f"{row.get('category_manifest_present','')} |"
        )
    lines.append("")
    lines.append("## 4. 功能回归检查")
    lines.append("")
    lines.append("| profile | boot_ok | debugfs_ok | tracing_ok | function_graph_ok | trace_smoke_ok | insmod_ok | snapshot_ok | trigger_ok | dmesg_warn |")
    lines.append("|---|---|---|---|---|---|---|---|---|---|")
    for row in rows:
        lines.append(
            f"| {row['profile']} | {row.get('boot_ok','')} | {row.get('debugfs_ok','')} | {row.get('tracing_ok','')} | "
            f"{row.get('function_graph_ok','')} | {row.get('trace_smoke_ok','')} | {row.get('insmod_ok','')} | "
            f"{row.get('snapshot_ok','')} | {row.get('trigger_ok','')} | {row.get('dmesg_warn','')} |"
        )
    lines.append("")
    lines.append("## 5. config diff 摘要")
    lines.append("")
    for key, summary in diff_summaries.items():
        lines.append(f"- **{key}**: {summary}; file={evidence_files[key]}")
    lines.append("")
    lines.append("## 6. 推荐读法")
    lines.append("")
    lines.append("1. 先看 status 和 remarks，确认有没有功能性回归。")
    lines.append("2. 再看 kernel_config_sha256 / kernel_savedefconfig_sha256 / kernel_image_sha256，判断三轮产物是否真的变化。")
    lines.append("3. 看 round2b_legacy vs classified 的 diff，确认 Day18 的分类表达是否只是‘说法不同’，还是最终配置也发生了变化。")
    lines.append("4. 如果 config sha 变化了，但 image sha 没变化，优先怀疑裁掉的 symbol 没有落到当前 virt+arm64 产物路径。")
    lines.append("5. 追细节时，先打开每轮 build_evidence/ 里的 kernel.config、kernel.savedefconfig、artifact_evidence.env。")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--records-root", default=str(RECORDS_ROOT))
    parser.add_argument("--profiles", nargs="*", default=DEFAULT_PROFILES)
    args = parser.parse_args()

    records_root = pathlib.Path(args.records_root)
    profiles = list(args.profiles)
    rows = build_rows(records_root, profiles)
    baseline = next((r for r in rows if r["profile"] == "baseline"), rows[0])

    baseline_cfg = pathlib.Path(baseline["record_dir"]) / "build_evidence" / "kernel.config"
    profile_to_record = {row["profile"]: pathlib.Path(row["record_dir"]) for row in rows}
    evidence_files: Dict[str, pathlib.Path] = {}
    diff_summaries: Dict[str, str] = {}

    comparisons = [
        ("baseline_vs_round2b_legacy", baseline_cfg, profile_to_record.get("round2b_legacy", pathlib.Path()) / "build_evidence" / "kernel.config"),
        ("baseline_vs_classified", baseline_cfg, profile_to_record.get("classified", pathlib.Path()) / "build_evidence" / "kernel.config"),
        ("round2b_legacy_vs_classified", profile_to_record.get("round2b_legacy", pathlib.Path()) / "build_evidence" / "kernel.config", profile_to_record.get("classified", pathlib.Path()) / "build_evidence" / "kernel.config"),
    ]

    for row in rows:
        if row["profile"] == baseline["profile"]:
            row["config_diff_vs_baseline"] = "self"
            row["image_hash_vs_baseline"] = "self"
            row["rootfs_hash_vs_baseline"] = "self"
            continue
        cfg_path = pathlib.Path(row["record_dir"]) / "build_evidence" / "kernel.config"
        summary, _ = config_diff_summary(baseline_cfg, cfg_path)
        row["config_diff_vs_baseline"] = summary
        row["image_hash_vs_baseline"] = "same" if row.get("kernel_image_sha256") == baseline.get("kernel_image_sha256") else "different"
        row["rootfs_hash_vs_baseline"] = "same" if row.get("rootfs_img_sha256") == baseline.get("rootfs_img_sha256") else "different"

    ts = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    csv_path = records_root / f"compare-{ts}.csv"
    md_path = records_root / f"compare-{ts}.md"

    for key, a, b in comparisons:
        summary, diff_lines = config_diff_summary(a, b)
        diff_summaries[key] = summary
        out_path = records_root / f"compare-{ts}-{key}.diff"
        write_diff(out_path, diff_lines, key)
        evidence_files[key] = out_path

    write_csv(csv_path, rows)
    write_md(md_path, rows, evidence_files, diff_summaries)
    print(csv_path)
    print(md_path)


if __name__ == "__main__":
    main()
