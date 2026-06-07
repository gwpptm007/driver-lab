# 01_LAB_OVERVIEW — 目标、流程与验收

## 定位

`project-af-xdp-mini-forwarder` 是 AF_XDP track 的第四站。目标是把前面三个 AF_XDP lab 的能力组合成一个项目型 mini forwarder：

```
XDP redirect → AF_XDP RX ring → userspace policy → recycle/drop/reflect → stats
```

前置能力：

```text
lab-xdp-redirect-basics      → XDP attach / action / redirect model
lab-af-xdp-socket-rings      → UMEM / XSK socket / rings 初始化
lab-af-xdp-zero-copy-vs-copy → copy / zero-copy mode 边界
```

## 范围

**范围内：**
- 单队列 AF_XDP socket (queue 0)
- `drop` 与 `reflect` 两种转发策略
- `skb + copy` 默认路径
- `native/zero-copy` 参数保留
- FILL → RX → TX → COMPLETION → FILL 完整 frame 生命周期
- stats / review bundle

**范围外（后续扩展）：**
- 多队列
- 双网卡 forward
- UDP-only 过滤 / checksum rewrite
- busy poll / need_wakeup 对比
- zero-copy 平台验证

## 执行流程

```bash
./scripts/00_check_env.sh                              # 环境检查
./scripts/01_build_app.sh                              # 编译 BPF + 用户态
[sudo AF_XDP_CONFIRM_REBIND=YES ./scripts/02_prepare_kernel_netdev.sh]  # 仅网卡被 DPDK 占用时需要
sudo AF_XDP_IFACE=veth-xdp ./scripts/03_run_forwarder_drop_smoke.sh      # drop 模式 smoke
sudo AF_XDP_IFACE=veth-xdp ./scripts/04_run_forwarder_reflect_smoke.sh   # reflect 模式 smoke
sudo AF_XDP_IFACE=veth-xdp ./scripts/06_collect_stats.sh                 # 收集统计
./scripts/07_make_review_bundle.sh                      # 生成报告
```

**veth pair + 流量注入方式（推荐）：**

```bash
# 创建 veth pair（如果还没有）
sudo ip link add veth-xdp type veth peer name veth-peer
sudo ip link set veth-xdp up && sudo ip link set veth-peer up
sudo ip addr add 10.99.0.2/24 dev veth-peer

# 后台运行 forwarder + 前台注入流量
sudo AF_XDP_IFACE=veth-xdp bash scripts/03_run_forwarder_drop_smoke.sh &
sleep 2
for i in $(seq 1 80); do
    ping -c 1 -W 0.02 -I veth-peer 10.99.0.1 2>/dev/null || true
done
wait
```

## 验收标准

### PASS_BUILD

```text
BUILD.log 中 BUILD_RESULT=PASS
build/af_xdp_forwarder 和 build/af_xdp_forwarder_kern.bpf.o 存在
```

### PASS_DROP_SMOKE

```text
FORWARDER_DROP.log 中:
  UMEM_READY 出现
  XSK_SOCKET_READY 出现
  AF_XDP_FORWARDER_READY mode=drop 出现
  FORWARDER_FINAL_STATS 出现
  程序正常退出 (bye)
```

### PASS_REFLECT_SMOKE

```text
FORWARDER_REFLECT.log 中:
  AF_XDP_FORWARDER_READY mode=reflect 出现
  FORWARDER_FINAL_STATS 出现
  程序正常退出 (bye)
```

### PASS_TRAFFIC

```text
FORWARDER_FINAL_STATS 中 rx_packets > 0
FORWARDER_FINAL_STATS 中 fill_recycled > 0
```

### PASS_TX_REFLECT

```text
FORWARDER_REFLECT 日志中:
  tx_packets > 0
  comp_packets > 0
```

无外部流量时允许先以 PASS_DROP_SMOKE + PASS_REFLECT_SMOKE 通过，PASS_TRAFFIC 和 PASS_TX_REFLECT 后续补测。

## 当前状态

- 测试日期: 2026-06-07
- 判定: **PASS_BUILD=YES, PASS_DROP_SMOKE=YES, PASS_REFLECT_SMOKE=YES, PASS_TRAFFIC=YES, PASS_TX_REFLECT=YES**
- 测试拓扑: veth pair (veth-peer → veth-xdp)
- DROP: rx_packets=3, dropped_packets=3, fill_recycled=3
- REFLECT: rx_packets=3, tx_packets=3, comp_packets=3 — 首次验证 TX 和 COMPLETION ring！
- 记录: `records/20260607-140717-af-xdp-mini-forwarder/`
