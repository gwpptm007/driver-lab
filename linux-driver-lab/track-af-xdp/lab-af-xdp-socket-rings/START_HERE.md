# START_HERE

## 环境准备

测试机 ens33 需要有和 ens192 同属 192.168.100.0/24 的 secondary IP，才能从本机触发 XDP redirect：

```bash
# 给 ens33 添加 secondary IP（已写入网卡启动配置，重启后依然生效）
sudo ip addr add 192.168.100.77/24 dev ens33

# 验证
ip addr show ens33 | grep 192.168.100.77
```

ens192 本身 IP 是 `192.168.100.1`，发送 UDP 到 `192.168.100.1:9000` 即可触发 RX。

---

## 流量测试（补测用）

在测试机终端 B 发送 UDP，同时终端 A 运行的 AF_XDP 程序在 poll：

```bash
# 终端 B：从 ens33 发往 ens192 的 IP（单机的 loopback 触发 XDP）
python3 -c 'import socket; s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM); [s.sendto(b"af-xdp",("192.168.100.1",9000)) for _ in range(2000)]'
```

---

## 一键顺序

```bash
cd track-af-xdp/lab-af-xdp-socket-rings

./scripts/00_check_env.sh
./scripts/01_build_app.sh
sudo AF_XDP_CONFIRM_REBIND=YES ./scripts/02_prepare_kernel_netdev.sh

# 先发包（终端 B），立刻启动收包（终端 A）
# 终端 B：python3 -c 'import socket; s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM); [s.sendto(b"af-xdp",("192.168.100.1",9000)) for _ in range(2000)]'
# 终端 A：
sudo AF_XDP_DURATION=30 ./scripts/03_run_af_xdp_socket_smoke.sh

./scripts/05_collect_stats.sh
./scripts/06_make_review_bundle.sh
```

## 关键日志

```text
records/*-af-xdp-socket-rings/ENV_CHECK.txt
records/*-af-xdp-socket-rings/BUILD.log
records/*-af-xdp-socket-rings/AF_XDP_SOCKET_SMOKE.log
records/*-af-xdp-socket-rings/COLLECT_STATS.txt
records/*-af-xdp-socket-rings/REVIEW_BUNDLE.md
```

## 关键字段

```text
UMEM_READY
XSK_SOCKET_READY
FILL_RING_READY
XDP_ATTACHED
XSKMAP_REGISTERED
AF_XDP_RINGS_READY
AF_XDP_FINAL_STATS
bye
```
