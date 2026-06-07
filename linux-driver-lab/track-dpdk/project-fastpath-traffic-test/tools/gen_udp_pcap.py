#!/usr/bin/env python3
"""Generate a pcap file with UDP packets for DPDK fastpath-lite testing.

Usage: python3 gen_udp_pcap.py [/path/to/output.pcap] [packet_count]

The generated pcap contains Ethernet/IPv4/UDP frames with:
  - dst_mac: 52:54:00:00:00:01
  - src_mac: 52:54:00:00:00:aa
  - src_ip:  10.10.10.1
  - dst_ip:  10.10.10.10
  - src_port: 12345
  - dst_port: 9000

These packets will be classified as ipv4 + udp by fastpath-lite.
"""
import socket
import struct
import sys


def write_pcap_global_header(f):
    """Write pcap global header (24 bytes), little-endian fields."""
    f.write(struct.pack('<IHHiIII',
        0xa1b2c3d4,  # magic number
        2,            # version major
        4,            # version minor
        0,            # thiszone (GMT)
        0,            # sigfigs
        65535,        # snaplen
        1,            # network (Ethernet)
    ))


def write_packet_record(f, ts_sec, ts_usec, data):
    """Write a pcap packet record: 16-byte header + packet data."""
    incl_len = len(data)
    f.write(struct.pack('<IIII', ts_sec, ts_usec, incl_len, incl_len))
    f.write(data)


def ipv4_checksum(header):
    """Compute IPv4 header checksum over 20 bytes (checksum field at offset 10 must be 0)."""
    total = 0
    for i in range(0, len(header), 2):
        w = (header[i] << 8) + header[i + 1]
        total += w
    while total >> 16:
        total = (total & 0xffff) + (total >> 16)
    return ~total & 0xffff


def build_udp_frame(dst_mac, src_mac, src_ip, dst_ip, src_port, dst_port, payload):
    """Build a complete Ethernet / IPv4 / UDP frame."""
    ip_total_len = 20 + 8 + len(payload)

    # IPv4 header: version=4, IHL=5, TTL=64, protocol=17 (UDP)
    ip_header = struct.pack('!BBHHHBBH4s4s',
        0x45,          # version=4, IHL=5 (20 bytes)
        0x00,          # DSCP / ECN
        ip_total_len,  # total length
        0x0001,        # identification
        0x0000,        # flags / fragment offset
        64,            # TTL
        17,            # protocol = UDP
        0x0000,        # header checksum (computed below)
        src_ip,        # source IP (4 bytes)
        dst_ip,        # destination IP (4 bytes)
    )

    # Compute IP checksum
    cksum = ipv4_checksum(ip_header[:10] + b'\x00\x00' + ip_header[12:])
    ip_header = ip_header[:10] + struct.pack('!H', cksum) + ip_header[12:]

    # UDP header: checksum = 0
    udp_len = 8 + len(payload)
    udp_header = struct.pack('!HHHH',
        src_port, dst_port, udp_len, 0x0000)

    # Ethernet header: EtherType = 0x0800 (IPv4)
    eth_header = dst_mac + src_mac + struct.pack('!H', 0x0800)

    return eth_header + ip_header + udp_header + payload


def main():
    output = sys.argv[1] if len(sys.argv) > 1 else '/tmp/udp_test.pcap'
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 200

    dst_mac = bytes.fromhex('520054000001')   # 52:54:00:00:00:01
    src_mac = bytes.fromhex('5200540000aa')   # 52:54:00:00:00:aa
    src_ip = socket.inet_aton('10.10.10.1')
    dst_ip = socket.inet_aton('10.10.10.10')
    payload = b'HELLO_UDP_FASTPATH_TEST_PACKET'

    with open(output, 'wb') as f:
        write_pcap_global_header(f)
        for i in range(count):
            pkt = build_udp_frame(dst_mac, src_mac, src_ip, dst_ip, 12345, 9000, payload)
            write_packet_record(f, i, 0, pkt)

    pkt_size = 14 + 20 + 8 + len(payload)
    print(f"Generated {count} UDP packets (each {pkt_size} bytes) -> {output}")


if __name__ == '__main__':
    main()
