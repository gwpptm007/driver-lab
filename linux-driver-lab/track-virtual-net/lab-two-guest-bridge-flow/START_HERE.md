# START_HERE

## 前置条件

建议先完成：

- `../lab-virtio-tap-bridge-path/`
- `../lab-virtio-vhost-kick-notify/`

最少要具备：

- QEMU 能启动 guest
- guest 内有 virtio_net
- host 能创建 tap / bridge
- 能在 guest 内手工配置 IP

## 推荐最小拓扑

```text
host:
  br-vnet0       192.168.100.1/24
  tap-vnet-a     attached to br-vnet0
  tap-vnet-b     attached to br-vnet0

guest A:
  eth0           192.168.100.2/24
  mac            52:54:00:12:34:a1

guest B:
  eth0           192.168.100.3/24
  mac            52:54:00:12:34:b1
```

## 最小开工流程

```bash
cd track-virtual-net/lab-two-guest-bridge-flow

./scripts/check_env.sh
REC=$(./scripts/bootstrap_record_dir.sh)
./scripts/plan_two_guest_bridge.sh
./scripts/generate_two_guest_qemu_args.sh
```

手工确认并执行 bridge/tap 命令，分别启动 guest A / guest B。

guest A 内：

```bash
ip addr add 192.168.100.2/24 dev eth0
ip link set eth0 up
```

guest B 内：

```bash
ip addr add 192.168.100.3/24 dev eth0
ip link set eth0 up
```

guest A ping guest B：

```bash
ping -c 5 192.168.100.3
```

host 侧采集：

```bash
./scripts/collect_two_guest_state.sh "$REC"
./scripts/capture_tcpdump_plan.sh
```

最后补：

- `SUMMARY.md`
- `TOPOLOGY_NOTE.md`
- `FLOW_REVIEW_NOTE.md`
