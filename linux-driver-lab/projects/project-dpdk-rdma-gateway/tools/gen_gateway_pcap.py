#!/usr/bin/env python3
import socket
import struct
import sys


def checksum(data):
    if len(data) % 2:
        data += b"\x00"
    total = sum((data[i] << 8) + data[i + 1] for i in range(0, len(data), 2))
    while total >> 16:
        total = (total & 0xFFFF) + (total >> 16)
    return (~total) & 0xFFFF


def ipv4_packet(index, udp):
    src = socket.inet_aton(f"10.1.0.{index % 200 + 1}")
    dst = socket.inet_aton("10.20.0.1")
    payload = f"GATEWAY_{'UDP' if udp else 'ICMP'}_{index:04d}".encode("ascii")
    payload = payload.ljust(32, b".")
    if udp:
        transport = struct.pack("!HHHH", 10000 + index, 20000,
                                8 + len(payload), 0) + payload
        protocol = 17
    else:
        transport = payload
        protocol = 1
    ip = struct.pack("!BBHHHBBH4s4s", 0x45, 0, 20 + len(transport),
                     index + 1, 0, 64, protocol, 0, src, dst)
    ip = ip[:10] + struct.pack("!H", checksum(ip)) + ip[12:]
    eth = bytes.fromhex("5200540000015200540000aa") + struct.pack("!H", 0x0800)
    return eth + ip + transport


def main():
    output = sys.argv[1]
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 64
    if count == 0 or count % 4:
        raise SystemExit("packet count must be a positive multiple of 4")
    with open(output, "wb") as stream:
        stream.write(struct.pack("<IHHiIII", 0xA1B2C3D4, 2, 4, 0, 0, 65535, 1))
        for index in range(count):
            # 每四包生成三包 UDP 和一包 ICMP，形成确定性分类比例。
            packet = ipv4_packet(index, index % 4 != 3)
            stream.write(struct.pack("<IIII", index, 0, len(packet), len(packet)))
            stream.write(packet)
    print(f"GATEWAY_PCAP_GENERATED packets={count} udp={count * 3 // 4} "
          f"unsupported={count // 4} output={output}")


if __name__ == "__main__":
    main()
