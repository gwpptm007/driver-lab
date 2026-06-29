#!/usr/bin/env python3
import socket
import struct
import sys


def global_header(f):
    f.write(struct.pack("<IHHiIII", 0xA1B2C3D4, 2, 4, 0, 0, 65535, 1))


def checksum(buf):
    if len(buf) % 2:
        buf += b"\x00"
    total = 0
    for i in range(0, len(buf), 2):
        total += (buf[i] << 8) + buf[i + 1]
    while total >> 16:
        total = (total & 0xFFFF) + (total >> 16)
    return (~total) & 0xFFFF


def packet(i, dst_ip, dst_port, tag):
    payload = f"L3_FORWARDER_{tag}_{i:04d}".encode()
    src = socket.inet_aton(f"10.10.{i % 32}.{1 + (i % 200)}")
    dst = socket.inet_aton(dst_ip)
    ip_len = 20 + 8 + len(payload)
    ip = struct.pack("!BBHHHBBH4s4s", 0x45, 0, ip_len, i & 0xFFFF, 0, 64, 17, 0, src, dst)
    ip = ip[:10] + struct.pack("!H", checksum(ip)) + ip[12:]
    udp = struct.pack("!HHHH", 10000 + (i % 1000), dst_port, 8 + len(payload), 0)
    eth = bytes.fromhex("5200540000015200540000aa") + struct.pack("!H", 0x0800)
    return eth + ip + udp + payload


def main():
    output = sys.argv[1] if len(sys.argv) > 1 else "/tmp/l3_input.pcap"
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 48
    with open(output, "wb") as f:
        global_header(f)
        for i in range(count):
            if i % 4 == 0:
                data = packet(i + 1, "10.20.0.77", 9999, "ACL_DROP")
            elif i % 4 == 1:
                data = packet(i + 1, "10.99.0.77", 9000, "ROUTE_MISS")
            else:
                data = packet(i + 1, "10.20.0.77", 9000, "FORWARD")
            f.write(struct.pack("<IIII", i, 0, len(data), len(data)))
            f.write(data)
    print(f"Generated {count} mixed IPv4/UDP packets -> {output}")


if __name__ == "__main__":
    main()

