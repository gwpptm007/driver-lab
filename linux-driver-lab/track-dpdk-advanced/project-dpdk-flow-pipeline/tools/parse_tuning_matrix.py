#!/usr/bin/env python3
import csv
import pathlib
import re
import sys


CONFIG_RE = re.compile(
    r"FLOW_CONFIG .* burst=(\d+) cache=(\d+) expected=(\d+) "
    r"extra_rules=(\d+) total_rules=(\d+)"
)
LATENCY_RE = re.compile(
    r"FLOW_LATENCY samples=(\d+) p50_cycles=(\d+) p99_cycles=(\d+) "
    r"max_cycles=(\d+) p99_ns=(\d+)"
)


def parse_log(path):
    text = path.read_text(encoding="utf-8", errors="replace")
    config = CONFIG_RE.search(text)
    latency = LATENCY_RE.search(text)
    if config is None or latency is None:
        raise ValueError(f"missing tuning marker: {path}")
    return {
        "case": path.stem,
        "burst": int(config.group(1)),
        "cache": int(config.group(2)),
        "packets": int(config.group(3)),
        "extra_rules": int(config.group(4)),
        "total_rules": int(config.group(5)),
        "samples": int(latency.group(1)),
        "p50_cycles": int(latency.group(2)),
        "p99_cycles": int(latency.group(3)),
        "max_cycles": int(latency.group(4)),
        "p99_ns": int(latency.group(5)),
    }


def main():
    directory = pathlib.Path(sys.argv[1])
    rows = [parse_log(path) for path in sorted(directory.glob("*.log"))]
    baseline = next(row for row in rows if row["case"] == "baseline")
    for row in rows:
        # 正数表示 p99 比 baseline 更高，负数表示更低；仅作当前环境相对比较。
        row["p99_vs_baseline_pct"] = round(
            (row["p99_cycles"] - baseline["p99_cycles"])
            * 100.0 / baseline["p99_cycles"], 2
        )

    fields = list(rows[0].keys())
    csv_path = directory / "TUNING_MATRIX.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)

    md_path = directory / "TUNING_MATRIX.md"
    with md_path.open("w", encoding="utf-8") as stream:
        stream.write("# DPDK Flow Pipeline Tuning Matrix\n\n")
        stream.write("| case | burst | cache | rules | samples | p50 cycles | p99 cycles | p99 ns | vs baseline |\n")
        stream.write("|---|---:|---:|---:|---:|---:|---:|---:|---:|\n")
        for row in rows:
            stream.write(
                f"| {row['case']} | {row['burst']} | {row['cache']} | "
                f"{row['total_rules']} | {row['samples']} | "
                f"{row['p50_cycles']} | {row['p99_cycles']} | "
                f"{row['p99_ns']} | {row['p99_vs_baseline_pct']}% |\n"
            )
        stream.write("\n> p99 只覆盖 parse + software hash lookup + decision。\n")
    print(f"FLOW_TUNING_PARSE_PASS cases={len(rows)} csv={csv_path} md={md_path}")


if __name__ == "__main__":
    main()
