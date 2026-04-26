#!/bin/bash
set -euo pipefail

KERNEL_IMG=/home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/output/x86/bzImage
REC=/home/wq7/records/vhost_test/off_test

cleanup() { pkill -9 qemu-system 2>/dev/null || true; }
trap cleanup EXIT

cleanup; sleep 1

timeout 35 qemu-system-x86_64 \
    -kernel "$KERNEL_IMG" \
    -nographic \
    -append "console=ttyS0" \
    -netdev tap,id=net0,ifname=tap-vnet0,script=no,downscript=no,vhost=off \
    -device virtio-net-pci,netdev=net0,mac=52:54:00:12:34:56 \
    > $REC/qemu_out.txt 2>&1 &

GUEST_PID=$!
sleep 8

# Send guest commands via stdio
{
    sleep 1
    echo "ip addr add 192.168.100.2/24 dev eth0"
    sleep 1
    echo "ip link set eth0 up"
    sleep 1
    echo "ping -c 5 192.168.100.1"
    sleep 5
} | timeout 25 qemu-system-x86_64 \
    -kernel "$KERNEL_IMG" \
    -nographic \
    -append "console=ttyS0" \
    -netdev tap,id=net0,ifname=tap-vnet0,script=no,downscript=no,vhost=off \
    -device virtio-net-pci,netdev=net0,mac=52:54:00:12:34:56 \
    < /dev/null \
    > /dev/null 2>&1 &

wait $GUEST_PID 2>/dev/null || true
sleep 1

ip -br link > $REC/ip_br_link.txt 2>&1 || true
ip addr > $REC/ip_addr.txt 2>&1 || true
bridge link > $REC/bridge_link.txt 2>&1 || true
bridge fdb show > $REC/bridge_fdb.txt 2>&1 || true
lsmod | grep -E 'vhost|tun|bridge' > $REC/modules.txt 2>&1 || true
ls -l /dev/vhost-net /dev/net/tun > $REC/dev_nodes.txt 2>&1 || true
ps -ef | grep '[q]emu' > $REC/qemu_process.txt 2>&1 || true

echo "=== off_test done ==="