#!/usr/bin/env python3
from __future__ import annotations

import argparse
import py_compile
import subprocess
from pathlib import Path
from typing import Dict, List, Tuple

DAY20_DIR = Path(__file__).resolve().parent
OUTPUT_DIR = DAY20_DIR / "output"
RECORDS_DIR = DAY20_DIR / "records"
STATUS_MD = OUTPUT_DIR / "day20_delivery_status.md"
STATUS_ENV = OUTPUT_DIR / "day20_delivery_status.env"

REQUIRED_FILES = [
    "README.md",
    "START_HERE.md",
    "FIRST_RUN.md",
    "DIRECTORY_TREE.md",
    "run_day20_regression.sh",
    "run_day20_regression.py",
    "run_day20_summary.sh",
    "run_day20_latest.sh",
    "inspect_day20_record.py",
    "summarize_day20_records.py",
    "guest/guest_day20_common.sh",
    "guest/guest_day20_smoke.sh",
    "guest/guest_day20_trace.sh",
    "guest/guest_day20_perf.sh",
    "guest/guest_day20_stress.sh",
    "docs/01_day20_plan.md",
    "docs/02_regression_items.md",
    "docs/03_script_architecture.md",
    "docs/04_acceptance.md",
    "docs/10_latest_and_verdict.md",
    "docs/11_failure_triage.md",
    "docs/14_final_wrapup.md",
    "FINAL_SUMMARY.md",
]

EXPECTED_OUTPUTS = [
    "output/day20_records_summary.csv",
    "output/day20_records_summary.md",
    "output/day20_latest_record.txt",
    "output/day20_latest_report.md",
    "output/day20_mode_summary.md",
    "output/day20_final_summary.md",
]


def bool01(v: bool) -> str:
    return "1" if v else "0"


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


def newest_record() -> Path | None:
    if not RECORDS_DIR.exists():
        return None
    items = sorted(p for p in RECORDS_DIR.iterdir() if p.is_dir() and "-day20-" in p.name)
    return items[-1] if items else None


def verify_shell(path: Path) -> Tuple[bool, str]:
    proc = subprocess.run(["bash", "-n", str(path)], capture_output=True, text=True)
    if proc.returncode == 0:
        return True, "ok"
    msg = (proc.stderr or proc.stdout).strip().replace("\n", " | ")
    return False, msg or "bash -n failed"


def verify_python(path: Path) -> Tuple[bool, str]:
    try:
        py_compile.compile(str(path), doraise=True)
        return True, "ok"
    except py_compile.PyCompileError as exc:
        return False, str(exc).replace("\n", " | ")


def collect_checks() -> Dict[str, object]:
    missing_files: List[str] = []
    shell_failures: List[str] = []
    python_failures: List[str] = []

    for rel in REQUIRED_FILES:
        if not (DAY20_DIR / rel).exists():
            missing_files.append(rel)

    for sh_path in sorted(DAY20_DIR.rglob("*.sh")):
        ok, msg = verify_shell(sh_path)
        if not ok:
            shell_failures.append(f"{sh_path.relative_to(DAY20_DIR)}: {msg}")

    for py_path in sorted(DAY20_DIR.rglob("*.py")):
        ok, msg = verify_python(py_path)
        if not ok:
            python_failures.append(f"{py_path.relative_to(DAY20_DIR)}: {msg}")

    output_missing = [rel for rel in EXPECTED_OUTPUTS if not (DAY20_DIR / rel).exists()]

    latest = newest_record()
    latest_name = latest.name if latest else "(none)"
    latest_summary = parse_env(latest / "summary.txt") if latest else {}
    latest_plan = parse_env(latest / "host_plan.env") if latest else {}

    runtime_ready = latest_summary.get("DRY_RUN_READY", "") == "1"
    regression_pass = latest_summary.get("REGRESSION_PASS", "") == "1"
    latest_mode = latest_summary.get("mode", latest_plan.get("mode", "unknown"))

    if latest is None:
        latest_verdict = "NO_RECORDS"
    elif "DRY_RUN_READY" in latest_summary:
        latest_verdict = "READY" if runtime_ready else "MISSING_INPUTS"
    elif regression_pass:
        latest_verdict = "PASS"
    elif latest_summary.get("REGRESSION_PASS", "") == "0":
        latest_verdict = "FAIL"
    else:
        latest_verdict = "UNKNOWN"

    suite_ready = not missing_files and not shell_failures and not python_failures
    delivery_ready = suite_ready and not output_missing

    return {
        "missing_files": missing_files,
        "shell_failures": shell_failures,
        "python_failures": python_failures,
        "output_missing": output_missing,
        "latest_record": latest_name,
        "latest_mode": latest_mode,
        "latest_verdict": latest_verdict,
        "missing_artifacts": latest_summary.get("MISSING_ARTIFACTS", ""),
        "fail_keys": latest_summary.get("FAIL_KEYS", ""),
        "missing_keys": latest_summary.get("MISSING_KEYS", ""),
        "suite_ready": suite_ready,
        "delivery_ready": delivery_ready,
        "runtime_ready": runtime_ready,
        "regression_pass": regression_pass,
    }


def write_outputs(data: Dict[str, object]) -> None:
    OUTPUT_DIR.mkdir(exist_ok=True)
    md_lines = [
        "# Day20 交付状态",
        "",
        "## 总体结论",
        "",
        f"- SUITE_READY: {bool01(bool(data['suite_ready']))}",
        f"- DELIVERY_READY: {bool01(bool(data['delivery_ready']))}",
        f"- RUNTIME_READY: {bool01(bool(data['runtime_ready']))}",
        f"- REGRESSION_PASS: {bool01(bool(data['regression_pass']))}",
        f"- latest_record: {data['latest_record']}",
        f"- latest_mode: {data['latest_mode']}",
        f"- latest_verdict: {data['latest_verdict']}",
        "",
        "## 怎么理解",
        "",
        "- SUITE_READY=1：目录结构、核心脚本、bash/python 语法自检都通过。",
        "- DELIVERY_READY=1：除 suite_ready 外，summary/latest 等日常入口产物也都已生成。",
        "- RUNTIME_READY=1：最近一次 dry-run 看到 image/rootfs/dtb/module 已齐，可以直接跑真实回归。",
        "- REGRESSION_PASS=1：最近一次真实回归已经判定通过。",
        "",
        "## 最近一次记录",
        "",
        f"- record_dir: {data['latest_record']}",
        f"- mode: {data['latest_mode']}",
        f"- verdict: {data['latest_verdict']}",
        f"- missing_artifacts: {data['missing_artifacts'] or '(none)'}",
        f"- fail_keys: {data['fail_keys'] or '(none)'}",
        f"- missing_keys: {data['missing_keys'] or '(none)'}",
        "",
        "## 结构与脚本检查",
        "",
    ]

    for title, key in [
        ("缺失文件", "missing_files"),
        ("shell 语法失败", "shell_failures"),
        ("python 语法失败", "python_failures"),
        ("缺失输出", "output_missing"),
    ]:
        items = data[key]  # type: ignore[index]
        md_lines.append(f"### {title}")
        md_lines.append("")
        if items:
            for item in items:  # type: ignore[assignment]
                md_lines.append(f"- {item}")
        else:
            md_lines.append("- (none)")
        md_lines.append("")

    md_lines.extend(
        [
            "## 建议动作",
            "",
            "1. 先执行 `./run_day20_summary.sh` 与 `./run_day20_latest.sh`，确认输出入口齐全。",
            "2. 如果 latest_verdict=MISSING_INPUTS，先补 image/rootfs/dtb/module，再跑 `MODE=all ./run_day20_regression.sh`。",
            "3. 如果 latest_verdict=FAIL，先看 `records/<record>/host_runner.log`，再看 `serial.log` 和 `summary.txt`。",
            "4. 每次修改 Day20 脚本后，至少重新跑一次 `./run_day20_verify.sh`。",
            "",
        ]
    )
    STATUS_MD.write_text("\n".join(md_lines), encoding="utf-8")

    env_lines = [
        f"SUITE_READY={bool01(bool(data['suite_ready']))}",
        f"DELIVERY_READY={bool01(bool(data['delivery_ready']))}",
        f"RUNTIME_READY={bool01(bool(data['runtime_ready']))}",
        f"REGRESSION_PASS={bool01(bool(data['regression_pass']))}",
        f"LATEST_RECORD={data['latest_record']}",
        f"LATEST_MODE={data['latest_mode']}",
        f"LATEST_VERDICT={data['latest_verdict']}",
        f"MISSING_ARTIFACTS={data['missing_artifacts']}",
        f"FAIL_KEYS={data['fail_keys']}",
        f"MISSING_KEYS={data['missing_keys']}",
    ]
    STATUS_ENV.write_text("\n".join(env_lines) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description="Verify Day20 suite structure and self-check status")
    parser.parse_args()
    data = collect_checks()
    write_outputs(data)
    print(f"[INFO] wrote {STATUS_MD}")
    print(f"[INFO] wrote {STATUS_ENV}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
