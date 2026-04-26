#!/bin/bash
set -euo pipefail

KERNEL_IMG=/home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/output/x86/bzImage
ROOTFS=/home/wq7/workspace/driver-lab/linux-driver-lab/foundation/day01/rootfs.img
REC=~/records/vhost_test

cleanup() { pkill -9 qemu-system 2>/dev/null || true; }
trap cleanup EXIT

run_test() {
    local vhost_flag=
    local label=
    cleanup; sleep 1

    mkdir -p /

    # Run QEMU with expect-like input for network setup
    (
        sleep 3
        echo ip addr add 192.168.100.2/24 dev eth0
        sleep 1
        echo ip link set eth0 up
        sleep 1
        echo ping -c 5 192.168.100.1
        sleep 6
    ) | timeout 25 qemu-system-x86_64         -kernel          -nographic         -append 'console=ttyS0'         -netdev tap,id=net0,ifname=tap-vnet0,script=no,downscript=no,vhost=         -device virtio-net-pci,netdev=net0,mac=52:54:00:12:34:56         -fsdev local,id=fs0,path=/home/wq7/workspace/driver-lab/kernel-src/busybox-1.36.1/output/x86/_install,security_model=mapped         > //qemu_out.txt 2>&1 &

    wait
    sleep 1

    # collect host state
    ip -br link > //ip_br_link.txt 2>&1 || true
    ip addr > //ip_addr.txt 2>&1 || true
    bridge link > //bridge_link.txt 2>&1 || true
    bridge fdb show > //bridge_fdb.txt 2>&1 || true
    lsmod | grep -E 'vhost|tun|bridge' > //modules.txt 2>&1 || true
    ls -l /dev/vhost-net /dev/net/tun > //dev_nodes.txt 2>&1 || true
    ps -ef | grep '[q]emu' > //qemu_process.txt 2>&1 || true
    echo === done ===
}

mkdir -p /off_test /on_test

echo === vhost=off ===
run_test off off_test

echo === vhost=on ===
run_test on on_test

echo ALL DONE
