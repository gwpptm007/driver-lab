#!/bin/bash
set -euo pipefail

KERNEL_IMG=/home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/output/x86/bzImage
ROOTFS=/home/wq7/workspace/driver-lab/linux-driver-lab/foundation/day01/rootfs.img
REC=/home/wq7/records/vhost_test

cleanup() { pkill -9 qemu-system 2>/dev/null || true; }
trap cleanup EXIT

run_test() {
    local vhost_flag=$1
    local label=$2
    cleanup; sleep 2

    mkdir -p $REC/$label

    echo "=== Running $label (vhost=$vhost_flag) ===" > $REC/$label/run.log

    # Use existing working rootfs.img with built-in init
    timeout 30 qemu-system-x86_64 \
        -kernel "$KERNEL_IMG" \
        -nographic \
        -append "console=ttyS0" \
        -netdev tap,id=net0,ifname=tap-vnet0,script=no,downscript=no,vhost=$vhost_flag \
        -device virtio-net-pci,netdev=net0,mac=52:54:00:12:34:56 \
        -initrd "$ROOTFS" \
        > $REC/$label/qemu_out.txt 2>&1 &

    wait
    sleep 1

    ip -br link > $REC/$label/ip_br_link.txt 2>&1 || true
    ip addr > $REC/$label/ip_addr.txt 2>&1 || true
    bridge link > $REC/$label/bridge_link.txt 2>&1 || true
    bridge fdb show > $REC/$label/bridge_fdb.txt 2>&1 || true
    lsmod | grep -E 'vhost|tun|bridge' > $REC/$label/modules.txt 2>&1 || true
    ls -l /dev/vhost-net /dev/net/tun > $REC/$label/dev_nodes.txt 2>&1 || true
    ps -ef | grep '[q]emu' > $REC/$label/qemu_process.txt 2>&1 || true

    echo "=== $label done ===" >> $REC/$label/run.log
}

mkdir -p $REC/off_test $REC/on_test

echo "Starting off test"
run_test off off_test
echo "Starting on test"
run_test on on_test

echo "ALL DONE"