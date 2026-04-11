#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path
from typing import Dict

DAY20_DIR = Path(__file__).resolve().parent
RECORDS_DIR = DAY20_DIR / "records"


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


def pick_record(name: str | None) -> Path:
    if name:
        p = RECORDS_DIR / name
        if not p.is_dir():
            raise SystemExit(f"[ERROR] record not found: {p}")
        return p
    items = sorted(p for p in RECORDS_DIR.iterdir() if p.is_dir() and "-day20-" in p.name)
    if not items:
        raise SystemExit("[ERROR] no day20 records found")
    return items[-1]


def verdict(summary: Dict[str, str]) -> str:
    if "DRY_RUN_READY" in summary:
        return "READY" if summary.get("DRY_RUN_READY") == "1" else "MISSING_INPUTS"
    if summary.get("REGRESSION_PASS") == "1":
        return "PASS"
    if summary.get("REGRESSION_PASS") == "0":
        return "FAIL"
    return "UNKNOWN"


def main() -> int:
    ap = argparse.ArgumentParser(description="Inspect one Day20 record")
    ap.add_argument("record_dir", nargs="?", help="record dir name under day20/records")
    args = ap.parse_args()

    record_dir = pick_record(args.record_dir)
    summary = parse_env(record_dir / "summary.txt")
    status = parse_env(record_dir / "pass_fail.env")
    plan = parse_env(record_dir / "host_plan.env")

    print(f"record_dir={record_dir.name}")
    print(f"mode={summary.get('mode', plan.get('mode', 'unknown'))}")
    print(f"verdict={verdict(summary)}")
    if "DRY_RUN_READY" in summary:
        print(f"dry_run_ready={summary.get('DRY_RUN_READY')}")
        print(f"missing_artifacts={summary.get('MISSING_ARTIFACTS', '')}")
    print(f"regression_pass={summary.get('REGRESSION_PASS', '')}")
    print(f"fail_keys={summary.get('FAIL_KEYS', '')}")
    print(f"missing_keys={summary.get('MISSING_KEYS', '')}")
    for rel in [
        "summary.txt", "pass_fail.env", "host_plan.env", "host_runner.log", "serial.log",
        "snapshot_before.txt", "snapshot_after.txt", "trace_excerpt.txt", "perf_stat.txt", "stress.log",
    ]:
        print(f"has_{rel.replace('.', '_')}={1 if (record_dir / rel).exists() else 0}")
    for key in [
        "DEBUGFS_OK", "DEMO_INSMOD_OK", "SNAPSHOT_OK", "TRIGGER_OK", "RMMOD_OK",
        "TRACING_OK", "FGRAPH_OK", "PERF_OK", "STRESS_OK", "DMESG_CLEAN",
    ]:
        if key in status:
            print(f"{key}={status[key]}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
