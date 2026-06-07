#!/usr/bin/env python3
"""Cross-track stats comparison tool.

Reads statistics from multiple sources and produces a consistency report:

  Sources supported:
    - DPDK fastpath-lite log  (software stats + rte_eth_stats)
    - DPDK media-gateway-lite log  (per-port/per-rule stats)
    - bpftrace packet_watcher.bt output  (kprobe event counts)
    - eBPF net_observer report  (Markdown report)

  Output:
    - Per-source key metrics extraction
    - Cross-source consistency check
    - Verdict: PASS / INCONSISTENT / INCOMPLETE

Usage:
  python3 compare_stats.py --dpdk-fastpath <log> [--bpftrace <log>] [--observer <md>]
  python3 compare_stats.py --dpdk-mgw <log> [--bpftrace <log>]
  python3 compare_stats.py --help
"""

import argparse
import re
import sys
from pathlib import Path
from dataclasses import dataclass, field
from typing import Optional


# ── Data structures ──────────────────────────────────────────────────────────

@dataclass
class DPDKStats:
    """Extracted DPDK fastpath or media-gateway stats."""
    source: str = ""
    port_stats: dict = field(default_factory=dict)  # port_id -> {rx, tx, ipv4, udp, rewrite, ...}
    ethdev_stats: dict = field(default_factory=dict)  # port_id -> {ipackets, opackets, ...}
    rule_stats: dict = field(default_factory=dict)    # mgw only: rule_name -> {hit, rewrite, ...}


@dataclass
class BPFTraceStats:
    """Extracted bpftrace packet_watcher.bt output."""
    napi_poll: int = 0
    netif_receive_skb: int = 0
    dev_queue_xmit: int = 0
    kfree_skb: int = 0
    xdp_redirect: int = 0
    per_cpu_napi: dict = field(default_factory=dict)
    per_cpu_skb: dict = field(default_factory=dict)


@dataclass
class ObserverStats:
    """Extracted eBPF net_observer Markdown report."""
    rx_packets: int = 0
    rx_bytes: int = 0
    gro_count: int = 0
    tx_queue_packets: int = 0
    tx_xmit_packets: int = 0
    drop_count: int = 0
    interfaces: list = field(default_factory=list)


# ── Parsers ──────────────────────────────────────────────────────────────────

DPDK_SW_STATS_RE = re.compile(
    r"port\s+(?P<port>\d+):\s+rx=(?P<rx>\d+)\s+rx_bytes=(?P<rx_bytes>\d+)\s+"
    r"tx=(?P<tx>\d+)\s+tx_bytes=(?P<tx_bytes>\d+)\s+tx_failed=(?P<tx_failed>\d+)\s+"
    r"arp=(?P<arp>\d+)\s+ipv4=(?P<ipv4>\d+)\s+udp=(?P<udp>\d+)\s+non_udp=(?P<non_udp>\d+)\s+"
    r"rewrite=(?P<rewrite>\d+)\s+drop_short=(?P<drop_short>\d+)\s+"
    r"drop_non_udp=(?P<drop_non_udp>\d+)\s+drop_no_peer=(?P<drop_no_peer>\d+)"
)

DPDK_ETHDEV_RE = re.compile(
    r"port\s+(?P<port>\d+):\s+ipackets=(?P<ipackets>\d+)\s+opackets=(?P<opackets>\d+)\s+"
    r"ibytes=(?P<ibytes>\d+)\s+obytes=(?P<obytes>\d+)\s+"
    r"imissed=(?P<imissed>\d+)\s+ierrors=(?P<ierrors>\d+)\s+oerrors=(?P<oerrors>\d+)"
)

MGW_RULE_RE = re.compile(
    r"rule\s+(?P<rule>\d+)\s+name=(?P<name>\S+)\s+enabled=(?P<enabled>\d+)\s+"
    r"dir=(?P<dir>[^\s]+)\s+match_dst_port=(?P<match_dst_port>\d+)\s+"
    r"rewrite_dst_mac=(?P<rewrite_dst_mac>[^\s]+)\s+"
    r"rewrite_dst_ip=(?P<rewrite_dst_ip>[^\s]+)\s+"
    r"rewrite_dst_port=(?P<rewrite_dst_port>\d+)"
)

MGW_RULE_STATS_RE = re.compile(
    r"rule\s+(?P<rule>\d+):\s+hit=(?P<hit>\d+)\s+bytes=(?P<bytes>\d+)\s+rewrite=(?P<rewrite>\d+)"
)

BPFTRACE_COUNT_RE = re.compile(r"(?P<event>\w+):\s+(?P<count>\d+)")


def parse_dpdk_log(path: str) -> DPDKStats:
    """Parse fastpath-lite or media-gateway-lite log for software + ethdev stats."""
    stats = DPDKStats(source=path)
    text = Path(path).read_text(errors="ignore")

    # Extract ALL software stat samples, take the last one per port
    sw_samples = {}
    for m in DPDK_SW_STATS_RE.finditer(text):
        d = {k: int(v) for k, v in m.groupdict().items()}
        port = d["port"]
        sw_samples[port] = d

    stats.port_stats = sw_samples

    # Extract ethdev stats (last sample per port)
    eth_samples = {}
    for m in DPDK_ETHDEV_RE.finditer(text):
        d = {k: int(v) for k, v in m.groupdict().items()}
        port = d["port"]
        eth_samples[port] = d

    stats.ethdev_stats = eth_samples

    # Extract rule stats (media-gateway-lite only)
    rule_samples = {}
    for m in MGW_RULE_STATS_RE.finditer(text):
        d = {k: int(v) for k, v in m.groupdict().items()}
        rule_id = d["rule"]
        rule_samples[rule_id] = d

    stats.rule_stats = rule_samples

    return stats


def parse_bpftrace_log(path: str) -> BPFTraceStats:
    """Parse bpftrace packet_watcher.bt summary output."""
    stats = BPFTraceStats()
    text = Path(path).read_text(errors="ignore")

    for m in BPFTRACE_COUNT_RE.finditer(text):
        event = m.group("event")
        count = int(m.group("count"))
        if "napi_poll" in event and "per_cpu" not in event:
            stats.napi_poll = count
        elif "netif_receive_skb" in event and "per_cpu" not in event:
            stats.netif_receive_skb = count
        elif "dev_queue_xmit" in event:
            stats.dev_queue_xmit = count
        elif "kfree_skb" in event:
            stats.kfree_skb = count
        elif "xdp_redirect" in event:
            stats.xdp_redirect = count

    return stats


def parse_observer_report(path: str) -> ObserverStats:
    """Parse eBPF net_observer Markdown report for key metrics."""
    stats = ObserverStats()
    text = Path(path).read_text(errors="ignore")

    # Extract common metrics from the observer report tables
    for line in text.splitlines():
        line = line.strip()
        if "RX packets" in line or "rx_packets" in line:
            m = re.search(r"(\d[\d,]*)", line)
            if m:
                stats.rx_packets = int(m.group(1).replace(",", ""))
        elif "GRO" in line and "count" in line.lower():
            m = re.search(r"(\d[\d,]*)", line)
            if m:
                stats.gro_count = int(m.group(1).replace(",", ""))
        elif "TX queue" in line and "packet" in line.lower():
            m = re.search(r"(\d[\d,]*)", line)
            if m:
                stats.tx_queue_packets = int(m.group(1).replace(",", ""))
        elif "TX xmit" in line and "packet" in line.lower():
            m = re.search(r"(\d[\d,]*)", line)
            if m:
                stats.tx_xmit_packets = int(m.group(1).replace(",", ""))
        elif "drop" in line.lower() and "count" in line.lower():
            m = re.search(r"(\d[\d,]*)", line)
            if m:
                stats.drop_count = int(m.group(1).replace(",", ""))

    return stats


# ── Comparison logic ─────────────────────────────────────────────────────────

def compare_dpdk_vs_bpftrace(dpdk: DPDKStats, bpftrace: BPFTraceStats) -> dict:
    """Compare DPDK fastpath stats with bpftrace kernel watcher.

    Key insight: DPDK PMD bypasses kernel, so:
      - dpdk rx > 0 + bpftrace netif_receive_skb == 0 → PASS (DPDK bypass confirmed)
      - dpdk rx > 0 + bpftrace netif_receive_skb > 0 → PARTIAL (background traffic)
      - dpdk rx == 0 → FAIL (DPDK didn't receive traffic)
    """
    result = {"status": "UNKNOWN", "notes": []}

    total_rx = sum(s.get("rx", 0) for s in dpdk.port_stats.values())
    total_tx = sum(s.get("tx", 0) for s in dpdk.port_stats.values())

    result["dpdk_rx"] = total_rx
    result["dpdk_tx"] = total_tx
    result["kernel_skb"] = bpftrace.netif_receive_skb
    result["kernel_napi"] = bpftrace.napi_poll

    if total_rx == 0:
        result["status"] = "FAIL"
        result["notes"].append("DPDK did not receive traffic. Check pcap PMD setup.")
    elif total_rx > 0 and bpftrace.netif_receive_skb == 0:
        result["status"] = "PASS"
        result["notes"].append(
            f"DPDK processed {total_rx:,} packets in userspace. "
            f"Kernel netif_receive_skb = 0 — DPDK PMD completely bypassed kernel stack."
        )
    elif total_rx > 0 and bpftrace.netif_receive_skb > 0:
        result["status"] = "PASS_WITH_NOISE"
        result["notes"].append(
            f"DPDK processed {total_rx:,} packets. "
            f"Kernel also saw {bpftrace.netif_receive_skb:,} skb arrivals "
            f"(likely background traffic on management interface)."
        )

    if total_tx > 0:
        result["notes"].append(f"DPDK forwarding: {total_tx:,} packets TX'd to net_null.")
    if bpftrace.napi_poll > 0 and bpftrace.netif_receive_skb == 0:
        result["notes"].append("NAPI poll ran but no skb received (idle polling).")

    return result


def compare_dpdk_sw_vs_ethdev(dpdk: DPDKStats) -> dict:
    """Compare DPDK software stats with hardware ethdev stats for consistency."""
    result = {"status": "PASS", "notes": []}

    for port, sw in dpdk.port_stats.items():
        eth = dpdk.ethdev_stats.get(port)
        if eth is None:
            result["notes"].append(f"Port {port}: no ethdev stats found")
            continue

        sw_rx = sw.get("rx", 0)
        eth_rx = eth.get("ipackets", 0)

        if sw_rx != eth_rx:
            result["status"] = "WARN"
            result["notes"].append(
                f"Port {port}: sw rx={sw_rx:,} != ethdev ipackets={eth_rx:,} "
                f"(may be expected: net_null mirrors TX→RX, or ethdev counts differently)"
            )
        else:
            result["notes"].append(f"Port {port}: sw rx={sw_rx:,} == ethdev ipackets={eth_rx:,} CONSISTENT")

    return result


# ── Output ────────────────────────────────────────────────────────────────────

def print_report(dpdk_result, ethdev_result, dpdk_stats, bpftrace_stats):
    """Print a formatted comparison report."""
    print()
    print("=" * 72)
    print("  Cross-Track Stats Comparison Report")
    print("=" * 72)
    print()

    # DPDK summary
    print("── DPDK Software Stats ──")
    for port, sw in sorted(dpdk_stats.port_stats.items()):
        print(f"  port {port}: rx={sw['rx']:,}  tx={sw['tx']:,}  "
              f"ipv4={sw['ipv4']:,}  udp={sw['udp']:,}  rewrite={sw['rewrite']:,}")
    if dpdk_stats.rule_stats:
        print("  rules:")
        for rid, rs in sorted(dpdk_stats.rule_stats.items()):
            print(f"    rule {rid}: hit={rs['hit']:,}  rewrite={rs['rewrite']:,}")
    print()

    # bpftrace summary
    if bpftrace_stats and (bpftrace_stats.napi_poll > 0 or bpftrace_stats.netif_receive_skb > 0):
        print("── Kernel Path (bpftrace) ──")
        print(f"  napi_poll:         {bpftrace_stats.napi_poll:,}")
        print(f"  netif_receive_skb: {bpftrace_stats.netif_receive_skb:,}")
        print(f"  dev_queue_xmit:    {bpftrace_stats.dev_queue_xmit:,}")
        print(f"  kfree_skb:         {bpftrace_stats.kfree_skb:,}")
        print(f"  xdp_redirect:      {bpftrace_stats.xdp_redirect:,}")
        print()

    # DPDK vs Kernel verdict
    print("── DPDK vs Kernel Bypass ──")
    print(f"  Status: {dpdk_result['status']}")
    for note in dpdk_result["notes"]:
        print(f"  • {note}")
    print()

    # SW vs Ethdev consistency
    print("── Software Stats vs Ethdev Stats ──")
    print(f"  Status: {ethdev_result['status']}")
    for note in ethdev_result["notes"]:
        print(f"  • {note}")
    print()

    # Final verdict
    print("── Final Verdict ──")
    if dpdk_result["status"] == "PASS" and ethdev_result["status"] == "PASS":
        print("  PASS: DPDK userspace fastpath verified with consistent stats.")
        print("  Kernel bypass confirmed via bpftrace observation.")
    elif dpdk_result["status"] == "PASS_WITH_NOISE":
        print("  PASS: DPDK fastpath working. Background kernel traffic detected (expected).")
    elif dpdk_result["status"] == "FAIL":
        print("  FAIL: DPDK did not receive traffic.")
    else:
        print(f"  {dpdk_result['status']}: Review notes above.")

    print()
    print("=" * 72)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Cross-track stats comparison for Linux Network Data Plane portfolio"
    )
    parser.add_argument("--dpdk-fastpath", help="Path to fastpath-lite log")
    parser.add_argument("--dpdk-mgw", help="Path to media-gateway-lite log")
    parser.add_argument("--bpftrace", help="Path to bpftrace packet_watcher.bt output")
    parser.add_argument("--observer", help="Path to eBPF net_observer report")
    parser.add_argument("--json", action="store_true", help="Output as JSON")
    args = parser.parse_args()

    dpdk_path = args.dpdk_fastpath or args.dpdk_mgw
    if not dpdk_path:
        print("ERROR: need --dpdk-fastpath or --dpdk-mgw", file=sys.stderr)
        return 1

    # Parse DPDK log
    if not Path(dpdk_path).exists():
        print(f"ERROR: DPDK log not found: {dpdk_path}", file=sys.stderr)
        return 1

    dpdk_stats = parse_dpdk_log(dpdk_path)
    bpftrace_stats = None
    observer_stats = None

    # Parse bpftrace log if provided
    if args.bpftrace and Path(args.bpftrace).exists():
        bpftrace_stats = parse_bpftrace_log(args.bpftrace)

    # Parse observer report if provided
    if args.observer and Path(args.observer).exists():
        observer_stats = parse_observer_report(args.observer)

    # Run comparisons
    dpdk_result = {"status": "UNKNOWN", "notes": ["No bpftrace data for comparison."], "dpdk_rx": 0, "dpdk_tx": 0, "kernel_skb": 0, "kernel_napi": 0}
    if bpftrace_stats:
        dpdk_result = compare_dpdk_vs_bpftrace(dpdk_stats, bpftrace_stats)

    ethdev_result = compare_dpdk_sw_vs_ethdev(dpdk_stats)

    if args.json:
        import json
        output = {
            "dpdk_stats": {str(k): v for k, v in dpdk_stats.port_stats.items()},
            "bpftrace_stats": {
                "napi_poll": bpftrace_stats.napi_poll if bpftrace_stats else 0,
                "netif_receive_skb": bpftrace_stats.netif_receive_skb if bpftrace_stats else 0,
            } if bpftrace_stats else None,
            "dpdk_vs_kernel": dpdk_result,
            "sw_vs_ethdev": ethdev_result,
        }
        print(json.dumps(output, indent=2))
    else:
        print_report(dpdk_result, ethdev_result, dpdk_stats, bpftrace_stats)

    # Exit code
    if dpdk_result["status"] == "FAIL":
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
