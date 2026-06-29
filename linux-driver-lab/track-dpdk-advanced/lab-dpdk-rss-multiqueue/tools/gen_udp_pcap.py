#!/usr/bin/env python3
"""Generate small UDP pcap input for RSS/multiqueue capability probe."""

import socket
import struct
import sys


def write_global_header(f):
    f.write(struct.pack("<IHHiIII", 0xA1B2C3D4, 2, 4, 0, 0, 65535, 1))


def checksum(header):
    total = 0
    for i in range(0, len(header), 2):
        total += (header[i] << 8) + header[i + 1]
    while total >> 16:
        total = (total & 0xFFFF) + (total >> 16)
    return (~total) & 0xFFFF


def frame(seq):
    dst_mac = bytes.fromhex("520054000001")
    src_mac = bytes.fromhex("5200540000aa")
    src_ip = socket.inet_aton(f"10.10.10.{1 + (seq % 8)}")
    dst_ip = socket.inet_aton("10.10.20.20")
    payload = f"RSS_QUEUE_SAMPLE_{seq:04d}".encode("ascii")
    ip_len = 20 + 8 + len(payload)
    ip = struct.pack("!BBHHHBBH4s4s", 0x45, 0, ip_len, seq & 0xFFFF, 0, 64, 17, 0, src_ip, dst_ip)
    csum = checksum(ip[:10] + b"\x00\x00" + ip[12:])
    ip = ip[:10] + struct.pack("!H", csum) + ip[12:]
    udp = struct.pack("!HHHH", 10000 + (seq % 16), 9000, 8 + len(payload), 0)
    eth = dst_mac + src_mac + struct.pack("!H", 0x0800)
    return eth + ip + udp + payload


def main():
    output = sys.argv[1] if len(sys.argv) > 1 else "/tmp/rss_input.pcap"
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 64
    with open(output, "wb") as f:
        write_global_header(f)
        for i in range(count):
            data = frame(i + 1)
            f.write(struct.pack("<IIII", i, 0, len(data), len(data)))
            f.write(data)
    print(f"Generated {count} UDP packets -> {output}")


if __name__ == "__main__":
    main()
