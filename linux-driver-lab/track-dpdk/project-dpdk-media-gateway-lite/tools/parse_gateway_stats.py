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
    totals = {k: 0 for k in KEYS}
    rule_hit = 0
    rule_rewrite = 0
    samples = 0

    for path in paths:
        text = path.read_text(errors="ignore")
        for line in text.splitlines():
            m = PORT_RE.search(line)
            if m:
                samples += 1
                for k in KEYS:
                    totals[k] += int(m.group(k))
                continue
            r = RULE_RE.search(line)
            if r:
                rule_hit += int(r.group("hit"))
                rule_rewrite += int(r.group("rewrite"))

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
