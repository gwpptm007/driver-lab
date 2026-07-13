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


def udp_packet(index, src_ip, dst_ip, src_port, dst_port, label):
    payload = f"FLOW_{label}_{index:04d}".encode("ascii")
    src = socket.inet_aton(src_ip)
    dst = socket.inet_aton(dst_ip)
    ip_length = 20 + 8 + len(payload)
    ip = struct.pack("!BBHHHBBH4s4s", 0x45, 0, ip_length, index, 0,
                     64, 17, 0, src, dst)
    ip = ip[:10] + struct.pack("!H", checksum(ip)) + ip[12:]
    udp = struct.pack("!HHHH", src_port, dst_port, 8 + len(payload), 0)
    eth = bytes.fromhex("5200540000015200540000aa") + struct.pack("!H", 0x0800)
    return eth + ip + udp + payload


def main():
    output = sys.argv[1] if len(sys.argv) > 1 else "/tmp/flow_input.pcap"
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 64
    flows = [
        ("10.1.0.1", "10.20.0.1", 10001, 20001, "DROP"),
        ("10.1.0.2", "10.20.0.2", 10002, 20002, "MARK"),
        ("10.1.0.3", "10.20.0.3", 10003, 20003, "FORWARD"),
        ("10.1.0.4", "10.20.0.4", 10004, 20004, "MISS"),
    ]
    # 四类流量轮转生成，确保每 4 个包形成一次动作计数守恒单元。
    with open(output, "wb") as stream:
        stream.write(struct.pack("<IHHiIII", 0xA1B2C3D4, 2, 4, 0, 0, 65535, 1))
        for index in range(count):
            flow = flows[index % len(flows)]
            packet = udp_packet(index + 1, *flow)
            stream.write(struct.pack("<IIII", index, 0, len(packet), len(packet)))
            stream.write(packet)
    print(f"FLOW_PCAP_GENERATED packets={count} classes=4 output={output}")


if __name__ == "__main__":
    main()
