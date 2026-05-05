#!/usr/bin/env python3
"""Small optional UDP traffic generator for fastpath-lite tests.

Run this on a different VM/host that can reach the DPDK-facing interface.
It intentionally depends on scapy only when you use it; the main project does
not require scapy.
"""

import argparse
import time


def main() -> int:
    parser = argparse.ArgumentParser(description="Send L2 UDP packets with scapy")
    parser.add_argument("--iface", required=True, help="eg: eth0")
    parser.add_argument("--dst-mac", required=True, help="destination MAC of DPDK port")
    parser.add_argument("--src-mac", default=None, help="optional source MAC")
    parser.add_argument("--dst-ip", required=True, help="destination IPv4")
    parser.add_argument("--src-ip", default="192.168.100.10", help="source IPv4")
    parser.add_argument("--sport", type=int, default=5000)
    parser.add_argument("--dport", type=int, default=6000)
    parser.add_argument("--payload", default="fastpath-lite-test")
    parser.add_argument("--count", type=int, default=10)
    parser.add_argument("--interval", type=float, default=0.1)
    args = parser.parse_args()

    from scapy.all import Ether, IP, UDP, Raw, sendp  # type: ignore

    ether_kwargs = {"dst": args.dst_mac}
    if args.src_mac:
        ether_kwargs["src"] = args.src_mac

    pkt = Ether(**ether_kwargs) / IP(src=args.src_ip, dst=args.dst_ip) / UDP(sport=args.sport, dport=args.dport) / Raw(args.payload.encode())

    for i in range(args.count):
        sendp(pkt, iface=args.iface, verbose=False)
        print(f"sent {i + 1}/{args.count}")
        time.sleep(args.interval)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
