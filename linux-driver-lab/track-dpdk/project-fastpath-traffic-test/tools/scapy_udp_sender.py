#!/usr/bin/env python3
"""Simple UDP traffic sender for fastpath-lite external traffic tests.

Run this on an external sender machine/VM, not on the same guest interface that
has already been bound to DPDK.
"""
import argparse
import time

try:
    from scapy.all import Ether, IP, UDP, Raw, sendp
except Exception as exc:  # pragma: no cover
    raise SystemExit(f"scapy import failed: {exc}\nInstall with: sudo apt install -y python3-scapy")


def parse_args():
    p = argparse.ArgumentParser()
    p.add_argument("--iface", required=True, help="sender interface")
    p.add_argument("--dst-mac", required=True, help="destination MAC, fastpath port MAC")
    p.add_argument("--src-mac", default=None, help="optional source MAC")
    p.add_argument("--src-ip", default="192.168.100.2")
    p.add_argument("--dst-ip", default="192.168.100.1")
    p.add_argument("--src-port", type=int, default=12345)
    p.add_argument("--dst-port", type=int, default=9000)
    p.add_argument("--count", type=int, default=1000)
    p.add_argument("--interval", type=float, default=0.0)
    p.add_argument("--payload", default="fastpath-traffic-test")
    return p.parse_args()


def main():
    args = parse_args()
    ether_kwargs = {"dst": args.dst_mac}
    if args.src_mac:
        ether_kwargs["src"] = args.src_mac
    pkt = (
        Ether(**ether_kwargs)
        / IP(src=args.src_ip, dst=args.dst_ip)
        / UDP(sport=args.src_port, dport=args.dst_port)
        / Raw(args.payload.encode())
    )
    print(f"sendp iface={args.iface} count={args.count} dst_mac={args.dst_mac} dst_ip={args.dst_ip}:{args.dst_port}")
    if args.interval <= 0:
        sendp(pkt, iface=args.iface, count=args.count, verbose=False)
    else:
        for _ in range(args.count):
            sendp(pkt, iface=args.iface, count=1, verbose=False)
            time.sleep(args.interval)
    print("done")


if __name__ == "__main__":
    main()
