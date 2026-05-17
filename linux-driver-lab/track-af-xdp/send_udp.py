#!/usr/bin/env python3
"""Send UDP packets to test AF_XDP socket"""

import socket
import time
import sys

def send_udp(dst_ip, dst_port, count=100, interval=0.01, payload_size=64):
    """Send UDP packets"""
    payload = b'X' * payload_size

    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        print(f"Sending {count} UDP packets to {dst_ip}:{dst_port} ({payload_size} bytes payload)")
        for i in range(count):
            sock.sendto(payload, (dst_ip, dst_port))
            if i % 100 == 0:
                print(f"  Sent {i}/{count}")
            time.sleep(interval)
        print(f"Done: {count} packets sent")
    except Exception as e:
        print(f"Error: {e}")
    finally:
        sock.close()

if __name__ == "__main__":
    # Default: send to ens192 network (192.168.100.1)
    ip = sys.argv[1] if len(sys.argv) > 1 else "192.168.100.1"
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 9999
    count = int(sys.argv[3]) if len(sys.argv) > 3 else 1000

    send_udp(ip, port, count)