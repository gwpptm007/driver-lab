#!/usr/bin/env python3
import socket
import struct
import sys


def gh(f):
    f.write(struct.pack("<IHHiIII", 0xA1B2C3D4, 2, 4, 0, 0, 65535, 1))


def csum(h):
    s = 0
    for i in range(0, len(h), 2):
        s += (h[i] << 8) + h[i + 1]
    while s >> 16:
        s = (s & 0xFFFF) + (s >> 16)
    return (~s) & 0xFFFF


def frame(i):
    payload = f"BURST_CACHE_SAMPLE_{i:06d}".encode()
    src = socket.inet_aton(f"10.10.{i % 64}.{1 + (i % 200)}")
    dst = socket.inet_aton("10.20.0.1")
    ip_len = 20 + 8 + len(payload)
    ip = struct.pack("!BBHHHBBH4s4s", 0x45, 0, ip_len, i & 0xFFFF, 0, 64, 17, 0, src, dst)
    ip = ip[:10] + struct.pack("!H", csum(ip[:10] + b"\x00\x00" + ip[12:])) + ip[12:]
    udp = struct.pack("!HHHH", 10000 + (i % 1000), 9000, 8 + len(payload), 0)
    eth = bytes.fromhex("5200540000015200540000aa") + struct.pack("!H", 0x0800)
    return eth + ip + udp + payload


def main():
    output = sys.argv[1] if len(sys.argv) > 1 else "/tmp/burst_input.pcap"
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 4096
    with open(output, "wb") as f:
        gh(f)
        for i in range(count):
            data = frame(i + 1)
            f.write(struct.pack("<IIII", i, 0, len(data), len(data)))
            f.write(data)
    print(f"Generated {count} UDP packets -> {output}")


if __name__ == "__main__":
    main()
