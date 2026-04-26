#!/bin/bash
set -euo pipefail

KDIR=/home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/build/x86
KERNEL_IMG=/home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/output/x86/bzImage
BUSYBOX=/home/wq7/workspace/driver-lab/kernel-src/busybox-1.36.1/output/x86/_install
REC=~/records/vhost_test

cleanup() { pkill -9 qemu-system 2>/dev/null || true; }
trap cleanup EXIT

collect_state() {
    local label=
    local out=/
    mkdir -p $out
    ip -br link > $out/ip_br_link.txt 2>&1 || true
    ip addr > $out/ip_addr.txt 2>&1 || true
    bridge link > $out/bridge_link.txt 2>&1 || true
    bridge fdb show > $out/bridge_fdb.txt 2>&1 || true
    lsmod | grep -E 'vhost|tun|bridge' > $out/modules.txt 2>&1 || true
    ls -l /dev/vhost-net /dev/net/tun > $out/dev_nodes.txt 2>&1 || true
    ps -ef | grep '[q]emu' > $out/qemu_process.txt 2>&1 || true
    echo "collected $label"
}

run_qemu() {
    local vhost_flag=$1
    local label=$2
    cleanup
    sleep 1
    qemu-system-x86_64         -kernel "$KERNEL_IMG"         -nographic         -append 'console=ttyS0'         -netdev tap,id=net0,ifname=tap-vnet0,script=no,downscript=no,vhost=$vhost_flag         -device virtio-net-pci,netdev=net0,mac=52:54:00:12:34:56         -fsdev local,id=fs0,path=$BUSYBOX,security_model=mapped         -fsdev local,id=fs1,path=$REC,security_model=mapped         -monitor none         -serial none         > $REC/$label/qemu.log 2>&1 &
    sleep 5
    # guest commands via QEMU monitor
    echo -e 'ip addr add 192.168.100.2/24 dev eth0\nip link set eth0 up\nping -c 5 192.168.100.1' | timeout 15 socat - TCP:localhost:4444,crlf 2>/dev/null || true
    collect_state $label
}

collect_state before
run_qemu off off_test
run_qemu on on_test

echo 'ALL DONE'
