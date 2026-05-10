#!/usr/bin/env python3
import re
import sys
from pathlib import Path

STAT_RE = re.compile(
    r"FORWARDER_(?:FINAL_)?STATS\s+"
    r"rx_packets=(?P<rx>\d+)\s+rx_bytes=(?P<rx_bytes>\d+)\s+"
    r"tx_packets=(?P<tx>\d+)\s+tx_bytes=(?P<tx_bytes>\d+)\s+"
    r"dropped_packets=(?P<drop>\d+)\s+fill_recycled=(?P<fill>\d+)\s+"
    r"tx_full_drops=(?P<tx_full>\d+)\s+comp_packets=(?P<comp>\d+)\s+"
    r"rx_empty_polls=(?P<empty>\d+)"
)

def parse_file(path: Path):
    last = None
    try:
        for line in path.read_text(errors='ignore').splitlines():
            m = STAT_RE.search(line)
            if m:
                last = {k: int(v) for k, v in m.groupdict().items()}
    except FileNotFoundError:
        return None
    return last

def main(argv):
    files = [Path(x) for x in argv[1:]]
    if not files:
        print("NO_INPUT_LOGS")
        return 0
    any_stat = False
    total_rx = total_tx = total_comp = 0
    for f in files:
        st = parse_file(f)
        if not st:
            continue
        any_stat = True
        total_rx += st['rx']
        total_tx += st['tx']
        total_comp += st['comp']
        print(f"{f.name}: rx={st['rx']} rx_bytes={st['rx_bytes']} tx={st['tx']} tx_bytes={st['tx_bytes']} drop={st['drop']} fill={st['fill']} tx_full={st['tx_full']} comp={st['comp']} empty={st['empty']}")
    if not any_stat:
        print("NO_FORWARDER_STATS_FOUND")
        return 1
    print(f"SUMMARY rx={total_rx} tx={total_tx} comp={total_comp}")
    print(f"PASS_TRAFFIC={'YES' if total_rx > 0 else 'NO'}")
    print(f"PASS_TX_REFLECT={'YES' if total_tx > 0 and total_comp > 0 else 'NO'}")
    return 0

if __name__ == '__main__':
    raise SystemExit(main(sys.argv))
