#!/usr/bin/env python3
from __future__ import annotations

import csv
from pathlib import Path
from typing import Dict, List

DAY20_DIR = Path(__file__).resolve().parent
RECORDS_DIR = DAY20_DIR / "records"
OUTPUT_DIR = DAY20_DIR / "output"
CSV_PATH = OUTPUT_DIR / "day20_records_summary.csv"
MD_PATH = OUTPUT_DIR / "day20_records_summary.md"
LATEST_TXT = OUTPUT_DIR / "day20_latest_record.txt"
LATEST_MD = OUTPUT_DIR / "day20_latest_report.md"
MODE_MD = OUTPUT_DIR / "day20_mode_summary.md"
INDEX_MD = OUTPUT_DIR / "day20_records_index.md"

COLUMNS = [
    "record_dir",
    "mode",
    "regression_pass",
    "fail_keys",
    "missing_keys",
    "dry_run_ready",
    "missing_artifacts",
    "debugfs_ok",
    "demo_insmod_ok",
    "snapshot_ok",
    "trigger_ok",
    "rmmod_ok",
    "tracing_ok",
    "fgraph_ok",
    "perf_ok",
    "stress_ok",
    "dmesg_clean",
]


def parse_env(path: Path) -> Dict[str, str]:
    data: Dict[str, str] = {}
    if not path.exists():
        return data
    for raw in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw.strip()
        if not line or "=" not in line:
            continue
        key, value = line.split("=", 1)
        data[key] = value
    return data


def collect_rows() -> List[Dict[str, str]]:
    rows: List[Dict[str, str]] = []
    if not RECORDS_DIR.exists():
        return rows
    record_dirs = [p for p in RECORDS_DIR.iterdir() if p.is_dir() and "-day20-" in p.name]
    for record_dir in sorted(record_dirs):
        summary = parse_env(record_dir / "summary.txt")
        status = parse_env(record_dir / "pass_fail.env")
        plan = parse_env(record_dir / "host_plan.env")
        rows.append(
            {
                "record_dir": record_dir.name,
                "mode": summary.get("mode", plan.get("mode", "unknown")),
                "regression_pass": summary.get("REGRESSION_PASS", ""),
                "fail_keys": summary.get("FAIL_KEYS", ""),
                "missing_keys": summary.get("MISSING_KEYS", ""),
                "dry_run_ready": summary.get("DRY_RUN_READY", ""),
                "missing_artifacts": summary.get("MISSING_ARTIFACTS", ""),
                "debugfs_ok": status.get("DEBUGFS_OK", ""),
                "demo_insmod_ok": status.get("DEMO_INSMOD_OK", ""),
                "snapshot_ok": status.get("SNAPSHOT_OK", ""),
                "trigger_ok": status.get("TRIGGER_OK", ""),
                "rmmod_ok": status.get("RMMOD_OK", ""),
                "tracing_ok": status.get("TRACING_OK", ""),
                "fgraph_ok": status.get("FGRAPH_OK", ""),
                "perf_ok": status.get("PERF_OK", ""),
                "stress_ok": status.get("STRESS_OK", ""),
                "dmesg_clean": status.get("DMESG_CLEAN", ""),
            }
        )
    return rows


def verdict_for_row(row: Dict[str, str]) -> str:
    if row.get("dry_run_ready"):
        return "READY" if row["dry_run_ready"] == "1" else "MISSING_INPUTS"
    if row.get("regression_pass") == "1":
        return "PASS"
    if row.get("regression_pass") == "0":
        return "FAIL"
    return "UNKNOWN"


def write_csv(rows: List[Dict[str, str]]) -> None:
    OUTPUT_DIR.mkdir(exist_ok=True)
    with CSV_PATH.open("w", encoding="utf-8", newline="") as fp:
        writer = csv.DictWriter(fp, fieldnames=COLUMNS)
        writer.writeheader()
        writer.writerows(rows)


def write_md(rows: List[Dict[str, str]]) -> None:
    total = len(rows)
    passed = sum(1 for r in rows if r.get("regression_pass") == "1")
    failed = sum(1 for r in rows if r.get("regression_pass") == "0")
    dry_runs = sum(1 for r in rows if r.get("dry_run_ready"))
    lines = [
        "# Day20 records 汇总",
        "",
        f"- total_records: {total}",
        f"- pass_records: {passed}",
        f"- fail_records: {failed}",
        f"- dry_run_records: {dry_runs}",
        "",
    ]
    if not rows:
        lines.extend(
            [
                "当前还没有真实的 Day20 records。",
                "",
                "建议先运行：",
                "",
                "```bash",
                "./run_day20_regression.sh --dry-run",
                "```",
                "",
            ]
        )
    else:
        latest = rows[-1]
        lines.extend(
            [
                "## 最近一次记录",
                "",
                f"- record_dir: {latest['record_dir']}",
                f"- mode: {latest['mode']}",
                f"- verdict: {verdict_for_row(latest)}",
                f"- regression_pass: {latest['regression_pass'] or '(n/a)'}",
                f"- fail_keys: {latest['fail_keys'] or '(none)'}",
                f"- missing_keys: {latest['missing_keys'] or '(none)'}",
                f"- missing_artifacts: {latest['missing_artifacts'] or '(none)'}",
                "",
                "## 按记录列出",
                "",
            ]
        )
        for row in rows:
            lines.extend(
                [
                    f"### {row['record_dir']}",
                    "",
                    f"- mode: {row['mode']}",
                    f"- verdict: {verdict_for_row(row)}",
                    f"- regression_pass: {row['regression_pass'] or '(n/a)'}",
                    f"- fail_keys: {row['fail_keys'] or '(none)'}",
                    f"- missing_keys: {row['missing_keys'] or '(none)'}",
                    f"- missing_artifacts: {row['missing_artifacts'] or '(none)'}",
                    "",
                ]
            )
    MD_PATH.write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_latest_outputs(rows: List[Dict[str, str]]) -> None:
    if not rows:
        LATEST_TXT.write_text("(none)\n", encoding="utf-8")
        LATEST_MD.write_text(
            "# Day20 最新结果\n\n当前还没有 Day20 record。先运行 `./run_day20_regression.sh --dry-run`。\n",
            encoding="utf-8",
        )
        return

    row = rows[-1]
    verdict = verdict_for_row(row)
    LATEST_TXT.write_text(row["record_dir"] + "\n", encoding="utf-8")
    lines = [
        "# Day20 最新结果",
        "",
        f"- record_dir: {row['record_dir']}",
        f"- mode: {row['mode']}",
        f"- verdict: {verdict}",
        "",
        "## 快速判断",
        "",
    ]
    if verdict == "READY":
        lines.extend(
            [
                "最近一次是 dry-run，而且输入件已经齐。下一步可以直接跑真实回归。",
                "",
                "```bash",
                "MODE=all ./run_day20_regression.sh",
                "```",
                "",
            ]
        )
    elif verdict == "MISSING_INPUTS":
        lines.extend(
            [
                "最近一次是 dry-run，但运行件还没齐。先补输入件，再跑真实回归。",
                "",
                f"- missing_artifacts: {row['missing_artifacts'] or '(unknown)'}",
                "",
            ]
        )
    elif verdict == "PASS":
        lines.extend(
            [
                "最近一次真实回归已经通过。建议打开该 record 的 `summary.txt` 和 `pass_fail.env` 复核。",
                "",
            ]
        )
    elif verdict == "FAIL":
        lines.extend(
            [
                "最近一次真实回归失败。先看 `host_runner.log` / `serial.log`，再看 `fail_keys` / `missing_keys`。",
                "",
                f"- fail_keys: {row['fail_keys'] or '(none)'}",
                f"- missing_keys: {row['missing_keys'] or '(none)'}",
                "",
            ]
        )
    else:
        lines.append("当前结论不明确，建议先看该 record 的 `summary.txt`。")
        lines.append("")

    lines.extend(
        [
            "## 关键状态",
            "",
            f"- DEBUGFS_OK: {row['debugfs_ok'] or '(n/a)'}",
            f"- DEMO_INSMOD_OK: {row['demo_insmod_ok'] or '(n/a)'}",
            f"- SNAPSHOT_OK: {row['snapshot_ok'] or '(n/a)'}",
            f"- TRIGGER_OK: {row['trigger_ok'] or '(n/a)'}",
            f"- RMMOD_OK: {row['rmmod_ok'] or '(n/a)'}",
            f"- TRACING_OK: {row['tracing_ok'] or '(n/a)'}",
            f"- FGRAPH_OK: {row['fgraph_ok'] or '(n/a)'}",
            f"- PERF_OK: {row['perf_ok'] or '(n/a)'}",
            f"- STRESS_OK: {row['stress_ok'] or '(n/a)'}",
            f"- DMESG_CLEAN: {row['dmesg_clean'] or '(n/a)'}",
            "",
            "## 下一步建议",
            "",
            "1. 打开 `records/<record_dir>/summary.txt` 看总判断。",
            "2. 打开 `records/<record_dir>/host_runner.log` 看宿主机侧阶段。",
            "3. 打开 `records/<record_dir>/serial.log` 看 guest 侧原始输出。",
            "4. 必要时再看 trace/perf/snapshot/dmesg 原始文本。",
            "",
        ]
    )
    LATEST_MD.write_text("\n".join(lines), encoding="utf-8")




def write_index(rows: List[Dict[str, str]]) -> None:
    lines = ["# Day20 records 索引", ""]
    if not rows:
        lines.extend(["当前没有 Day20 records。", ""])
    else:
        lines.extend(["| record_dir | mode | verdict | fail_keys | missing_artifacts |", "|---|---|---|---|---|"])
        for row in reversed(rows):
            lines.append(
                f"| {row['record_dir']} | {row['mode']} | {verdict_for_row(row)} | {row['fail_keys'] or '(none)'} | {row['missing_artifacts'] or '(none)'} |"
            )
        lines.append("")
    INDEX_MD.write_text("\n".join(lines) + "\n", encoding="utf-8")

def write_mode_summary(rows: List[Dict[str, str]]) -> None:
    mode_map: Dict[str, Dict[str, int]] = {}
    for row in rows:
        mode = row.get("mode") or "unknown"
        bucket = mode_map.setdefault(mode, {"total": 0, "pass": 0, "fail": 0, "dry_run": 0})
        bucket["total"] += 1
        verdict = verdict_for_row(row)
        if verdict == "PASS":
            bucket["pass"] += 1
        elif verdict == "FAIL":
            bucket["fail"] += 1
        elif verdict in {"READY", "MISSING_INPUTS"}:
            bucket["dry_run"] += 1
    lines = ["# Day20 按 mode 汇总", ""]
    if not mode_map:
        lines.append("当前没有可汇总的 Day20 record。")
        lines.append("")
    else:
        lines.extend(["| mode | total | pass | fail | dry_run |", "|---|---:|---:|---:|---:|"])
        for mode in sorted(mode_map):
            bucket = mode_map[mode]
            lines.append(
                f"| {mode} | {bucket['total']} | {bucket['pass']} | {bucket['fail']} | {bucket['dry_run']} |"
            )
        lines.append("")
    MODE_MD.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    rows = collect_rows()
    OUTPUT_DIR.mkdir(exist_ok=True)
    write_csv(rows)
    write_md(rows)
    write_latest_outputs(rows)
    write_mode_summary(rows)
    write_index(rows)
    print(f"[INFO] wrote: {CSV_PATH}")
    print(f"[INFO] wrote: {MD_PATH}")
    print(f"[INFO] wrote: {LATEST_TXT}")
    print(f"[INFO] wrote: {LATEST_MD}")
    print(f"[INFO] wrote: {MODE_MD}")
    print(f"[INFO] wrote: {INDEX_MD}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
