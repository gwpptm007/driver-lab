# 01_LAB_OVERVIEW — 目标、流程与验收

## 定位

`lab-af-xdp-socket-rings` 是 AF_XDP track 的第二站。目标是把 AF_XDP 的七个核心对象串联起来：

```text
XDP program → XSKMAP → AF_XDP socket → UMEM → FILL ring → RX ring → 用户态收包
                                         (TX ring + COMPLETION ring 预留)
```

重点不是性能，而是验证完整的数据闭环。

## 范围

**范围内：**
- AF_XDP socket 创建 (XDP_COPY 模式)
- UMEM 分配与 frame 管理 (4096 frames, 2048 bytes/frame, 8MB)
- FILL/RX/TX/COMPLETION 四类 ring 初始化
- XSKMAP 注册 (queue 0 → socket fd)
- XDP redirect → AF_XDP socket → 用户态 poll 收包
- BPF per-CPU stats map 统计

**范围外（后续 lab/project）：**
- 多队列并发
- zero-copy 性能对比
- L2 forwarding / 用户态发包
- busy-poll / need-wakeup 调优

## 执行流程

```bash
./scripts/00_check_env.sh                              # 环境检查
./scripts/01_build_app.sh                              # 编译 BPF + 用户态
[sudo AF_XDP_CONFIRM_REBIND=YES ./scripts/02_prepare_kernel_netdev.sh]  # 仅网卡被 DPDK 占用时需要
sudo AF_XDP_IFACE=veth-xdp ./scripts/03_run_af_xdp_socket_smoke.sh      # 运行 AF_XDP 收包
./scripts/05_collect_stats.sh                          # 收集统计
./scripts/06_make_review_bundle.sh                     # 生成报告
```

**veth pair 测试拓扑（推荐）：**

```bash
# 创建
sudo ip link add veth-xdp type veth peer name veth-peer
sudo ip link set veth-xdp up && sudo ip link set veth-peer up
sudo ip addr add 10.99.0.2/24 dev veth-peer

# 后台运行 AF_XDP 收包 + 前台注入流量
sudo AF_XDP_IFACE=veth-xdp bash scripts/03_run_af_xdp_socket_smoke.sh &
sleep 2
for i in $(seq 1 80); do
    ping -c 1 -W 0.02 -I veth-peer 10.99.0.1 2>/dev/null || true
done
wait
```

## 验收标准

### PASS_SOCKET_READY

```text
AF_XDP_SOCKET_SMOKE.log 中:
  XSK_SOCKET_READY 出现
  XSKMAP_REGISTERED 出现
  程序正常退出 (bye)
```

### PASS_UMEM_RINGS

```text
AF_XDP_SOCKET_SMOKE.log 中:
  UMEM_READY 出现 (frames=4096, frame_size=2048, bytes=8388608)
  FILL_RING_READY 出现 (descriptors=2048)
  AF_XDP_RINGS_READY 出现 (fq=2048, cq=2048, rx=2048, tx=2048)
```

### PASS_RX_TRAFFIC

```text
AF_XDP_FINAL_STATS 中 rx_packets > 0
```

无外部流量时允许先以 PASS_SOCKET_READY + PASS_UMEM_RINGS 作为 smoke 通过，PASS_RX_TRAFFIC 后续补测。

## 当前状态

- 测试日期: 2026-06-07
- 判定: **PASS_SOCKET_READY=YES, PASS_UMEM_RINGS=YES, PASS_RX_TRAFFIC=YES**
- 测试拓扑: veth pair (veth-peer → veth-xdp)
- 收包: rx_packets=3 (首轮 49 pkts, 6663 bytes)
- 记录: `records/20260607-135550-af-xdp-socket-rings/`
