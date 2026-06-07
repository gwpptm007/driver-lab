#!/usr/bin/env python3
"""Parse fastpath-lite software stats from logs and print verdict hints."""
import re
import sys
from pathlib import Path

STAT_RE = re.compile(
    r"port\s+(?P<port>\d+):\s+rx=(?P<rx>\d+)\s+rx_bytes=(?P<rx_bytes>\d+)\s+"
    r"tx=(?P<tx>\d+)\s+tx_bytes=(?P<tx_bytes>\d+)\s+tx_failed=(?P<tx_failed>\d+)\s+"
    r"arp=(?P<arp>\d+)\s+ipv4=(?P<ipv4>\d+)\s+udp=(?P<udp>\d+)\s+non_udp=(?P<non_udp>\d+)\s+"
    r"rewrite=(?P<rewrite>\d+)\s+drop_short=(?P<drop_short>\d+)\s+drop_non_udp=(?P<drop_non_udp>\d+)\s+drop_no_peer=(?P<drop_no_peer>\d+)"
)


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: parse_fastpath_stats.py FASTPATH_RX.log")
        return 2
    path = Path(sys.argv[1])
    if not path.exists():
        print(f"log not found: {path}")
        return 1
    matches = []
    for line in path.read_text(errors="ignore").splitlines():
        m = STAT_RE.search(line)
        if m:
            matches.append({k: int(v) for k, v in m.groupdict().items()})
    if not matches:
        print("verdict=UNKNOWN no fastpath software stats found")
        return 0
    # Aggregate stats across all ports for verdict determination
    agg = {"rx": 0, "tx": 0, "ipv4": 0, "udp": 0, "rewrite": 0, "arp": 0, "non_udp": 0,
           "drop_short": 0, "drop_non_udp": 0, "drop_no_peer": 0, "tx_failed": 0}
    for m in matches:
        for k in agg:
            agg[k] += m.get(k, 0)

    last = matches[-1]
    print("last_stats=" + " ".join(f"{k}={v}" for k, v in last.items()))
    print("agg_stats=" + " ".join(f"{k}={v}" for k, v in agg.items()))

    verdicts = []
    if agg["rx"] == 0:
        verdicts.append("PASS_SMOKE_ONLY")
    else:
        verdicts.append("PASS_SMOKE")
        if agg["udp"] > 0 or agg["ipv4"] > 0:
            verdicts.append("PASS_TRAFFIC")
        if agg["tx"] > 0:
            verdicts.append("PASS_FORWARDING")
        if agg["rewrite"] > 0:
            verdicts.append("PASS_REWRITE")
        if not any(v in verdicts for v in ["PASS_TRAFFIC", "PASS_FORWARDING", "PASS_REWRITE"]):
            verdicts.append("PASS_TRAFFIC_PARTIAL (rx_nonzero_but_protocol_counters_zero)")

    print("verdict=" + " ".join(verdicts))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
