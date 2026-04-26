#!/bin/bash
set -euo pipefail

KERNEL_IMG=/home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/output/x86/bzImage
BUSYBOX=/home/wq7/workspace/driver-lab/kernel-src/busybox-1.36.1/output/x86/_install
REC=~/records/vhost_test

cleanup() { pkill -9 qemu-system 2>/dev/null || true; }
trap cleanup EXIT

collect_state() {
    local label=
    local out=/
    mkdir -p 
    ip -br link > /ip_br_link.txt 2>&1 || true
    ip addr > /ip_addr.txt 2>&1 || true
    bridge link > /bridge_link.txt 2>&1 || true
    bridge fdb show > /bridge_fdb.txt 2>&1 || true
    lsmod | grep -E 'vhost|tun|bridge' > /modules.txt 2>&1 || true
    ls -l /dev/vhost-net /dev/net/tun > /dev_nodes.txt 2>&1 || true
    ps -ef | grep '[q]emu' > /qemu_process.txt 2>&1 || true
}

run_test() {
    local vhost_flag=
    local label=
    cleanup; sleep 1

    # Create a fifo for sending commands
    local fifo=/fifo_
    rm -f 
    mkfifo 

    # Start QEMU with console output going to a log
    qemu-system-x86_64         -kernel          -nographic         -append 'console=ttyS0'         -netdev tap,id=net0,ifname=tap-vnet0,script=no,downscript=no,vhost=         -device virtio-net-pci,netdev=net0,mac=52:54:00:12:34:56         -fsdev local,id=fs0,path=,security_model=mapped         -fsdev local,id=fs1,path=,security_model=mapped         > //qemu_boot.log 2>&1 &

    local qemu_pid=
    sleep 4

    # Send commands to guest via stdin's console
    {
        sleep 2
        echo ip addr add 192.168.100.2/24 dev eth0
        sleep 1
        echo ip link set eth0 up
        sleep 1
        echo ping -c 5 192.168.100.1
        sleep 5
        echo exit
    } >  &

    # Feed fifo to QEMU's serial console
    timeout 20 cat  > /dev/null 2>&1 || true

    wait  2>/dev/null || true
    sleep 1
    collect_state 
}

mkdir -p /off_test /on_test

echo === Starting vhost=off test ===
run_test off off_test
echo off_test done

echo === Starting vhost=on test ===
run_test on on_test
echo on_test done

echo === ALL DONE ===
ls -la /
