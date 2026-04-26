# 02_EXECUTION_FLOW

## Step 1：检查环境

```bash
./scripts/check_env.sh
```

## Step 2：创建 records

```bash
REC=$(./scripts/bootstrap_record_dir.sh)
```

## Step 3：创建 bridge + two taps

```bash
# 创建 bridge
echo wq123456! | sudo -S ip link add name br-vnet0 type bridge
echo wq123456! | sudo -S ip addr add 192.168.100.1/24 dev br-vnet0
echo wq123456! | sudo -S ip link set br-vnet0 up

# 创建两个 tap
echo wq123456! | sudo -S ip tuntap add dev tap-vnet-a mode tap user $(whoami)
echo wq123456! | sudo -S ip link set tap-vnet-a master br-vnet0
echo wq123456! | sudo -S ip link set tap-vnet-a up

echo wq123456! | sudo -S ip tuntap add dev tap-vnet-b mode tap user $(whoami)
echo wq123456! | sudo -S ip link set tap-vnet-b master br-vnet0
echo wq123456! | sudo -S ip link set tap-vnet-b up
```

## Step 4：启动 guest A

guest A（先启动，A 会把 tap-vnet-a 拉 UP）：

```text
-netdev tap,id=net0,ifname=tap-vnet-a,script=no,downscript=no
-device virtio-net-pci,netdev=net0,mac=52:54:00:12:34:a1
```

guest 内：
```bash
ip addr add 192.168.100.2/24 dev eth0
ip link set eth0 up
```

## Step 5：启动 guest B

guest B：

```text
-netdev tap,id=net0,ifname=tap-vnet-b,script=no,downscript=no
-device virtio-net-pci,netdev=net0,mac=52:54:00:12:34:b1
```

guest 内：
```bash
ip addr add 192.168.100.3/24 dev eth0
ip link set eth0 up
```

## Step 6：guest A ping guest B

```bash
ping -c 5 192.168.100.3
```

## Step 7：采集 host 状态

```bash
ip -br link > $REC/host/ip_br_link.txt
bridge link > $REC/host/bridge_link.txt
bridge fdb show > $REC/host/bridge_fdb.txt
lsmod | grep -E 'vhost|tun|bridge' > $REC/host/modules.txt
```

## Step 8：补结论

补：
- `SUMMARY.md`
- `TOPOLOGY_NOTE.md`
- `FLOW_REVIEW_NOTE.md`
