#!/usr/bin/env python3
"""Parse media-gateway-lite stats logs and print verdict hints."""
from __future__ import annotations

import re
import sys
from pathlib import Path

PORT_RE = re.compile(
    r"port\s+(?P<port>\d+):\s+rx=(?P<rx>\d+)\s+rx_bytes=(?P<rx_bytes>\d+)\s+"
    r"tx=(?P<tx>\d+)\s+tx_bytes=(?P<tx_bytes>\d+)\s+tx_failed=(?P<tx_failed>\d+)\s+"
    r"drops=(?P<drops>\d+)\s+arp=(?P<arp>\d+)\s+ipv4=(?P<ipv4>\d+)\s+udp=(?P<udp>\d+)\s+non_udp=(?P<non_udp>\d+)\s+"
    r"rewrite=(?P<rewrite>\d+)\s+drop_short=(?P<drop_short>\d+)\s+drop_non_udp=(?P<drop_non_udp>\d+)\s+drop_no_route=(?P<drop_no_route>\d+)"
)
RULE_RE = re.compile(
    r"rule\s+(?P<rule>\d+):\s+hit=(?P<hit>\d+)\s+bytes=(?P<bytes>\d+)\s+rewrite=(?P<rewrite>\d+)"
)

KEYS = [
    "rx", "rx_bytes", "tx", "tx_bytes", "tx_failed", "drops", "arp", "ipv4", "udp",
    "non_udp", "rewrite", "drop_short", "drop_non_udp", "drop_no_route",
]


def parse(paths: list[Path]):
    """解析统计日志，取每个 port/rule 的最后一次采样值。

    网关按周期打印的是累计计数器，不是增量。
    所以取最后一次采样才是正确的总计，而不是累加所有采样。

    注意：每个 port 下面都会打印 rule_hit/rule_rewrite（per-port 独立计数），
    需要按 (port_id, rule_id) 追踪，最后跨 port 汇总。
    """
    port_last = {}             # port_id -> {key: value}
    rule_hit_last = {}         # (port_id, rule_id) -> hit
    rule_rewrite_last = {}     # (port_id, rule_id) -> rewrite
    current_port = None        # 当前解析的 port 上下文
    samples = 0

    for path in paths:
        text = path.read_text(errors="ignore")
        for line in text.splitlines():
            m = PORT_RE.search(line)
            if m:
                samples += 1
                port_id = int(m.group("port"))
                current_port = port_id
                port_last[port_id] = {k: int(m.group(k)) for k in KEYS}
                continue
            r = RULE_RE.search(line)
            if r and current_port is not None:
                rule_id = int(r.group("rule"))
                rule_hit_last[(current_port, rule_id)] = int(r.group("hit"))
                rule_rewrite_last[(current_port, rule_id)] = int(r.group("rewrite"))

    # 汇总所有 port 的最后一次采样值
    totals = {k: 0 for k in KEYS}
    for pd in port_last.values():
        for k in KEYS:
            totals[k] += pd[k]
    rule_hit = sum(rule_hit_last.values())
    rule_rewrite = sum(rule_rewrite_last.values())

    return totals, rule_hit, rule_rewrite, samples


def main(argv: list[str]) -> int:
    if len(argv) < 2:
        print("usage: parse_gateway_stats.py LOG...", file=sys.stderr)
        return 2
    paths = [Path(x) for x in argv[1:]]
    totals, rule_hit, rule_rewrite, samples = parse(paths)

    print(f"samples={samples}")
    for k in KEYS:
        print(f"{k}={totals[k]}")
    print(f"rule_hit={rule_hit}")
    print(f"rule_rewrite={rule_rewrite}")
    print()
    print("verdict_hints:")
    print(f"PASS_SMOKE={'YES' if samples > 0 else 'NO'}")
    print(f"PASS_TRAFFIC={'YES' if totals['rx'] > 0 and (totals['ipv4'] > 0 or totals['udp'] > 0) else 'NO'}")
    print(f"PASS_FORWARDING={'YES' if totals['tx'] > 0 else 'NO'}")
    print(f"PASS_REWRITE={'YES' if totals['rewrite'] > 0 or rule_rewrite > 0 else 'NO'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
