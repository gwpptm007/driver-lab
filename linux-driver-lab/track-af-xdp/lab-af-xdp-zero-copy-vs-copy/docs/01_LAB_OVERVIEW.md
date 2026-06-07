# 01_LAB_OVERVIEW — 目标、流程与验收

## 定位

`lab-af-xdp-zero-copy-vs-copy` 是 AF_XDP track 的第三站。目标不是做性能压测，而是验证 AF_XDP 的 copy mode 和 zero-copy mode 的**驱动支持边界**：

- `skb + copy` 是否能作为基线启动
- `native + copy` 是否受驱动支持
- `native + zero-copy` 是否受驱动支持
- zero-copy 失败时如何记录和 fallback

## 范围

**范围内：**
- XDP attach mode (skb/native) 和 AF_XDP bind mode (copy/zero-copy) 的 2x2 组合探测
- `af_xdp_mode_probe` 程序运行三种模式组合并记录返回码
- COMPARE_MODES.txt 分类判定：PASS / UNSUPPORTED / ATTACH_FAIL
- fallback 策略记录

**范围外（后续 lab/project）：**
- 吞吐性能压测
- 多队列 RSS
- 完整用户态转发器
- 把 zero-copy 失败当成实验失败

## 执行流程

```bash
./scripts/00_check_env.sh                              # 环境检查
./scripts/01_build_app.sh                              # 编译 BPF + 用户态
[sudo AF_XDP_CONFIRM_REBIND=YES ./scripts/02_prepare_kernel_netdev.sh]  # 仅网卡被 DPDK 占用时需要
sudo AF_XDP_IFACE=veth-xdp ./scripts/03_run_copy_mode_baseline.sh       # skb+copy 基线
sudo AF_XDP_IFACE=veth-xdp ./scripts/04_probe_native_copy.sh             # native+copy 探测
sudo AF_XDP_IFACE=veth-xdp ./scripts/05_probe_zero_copy.sh               # native+zero-copy 探测
sudo AF_XDP_IFACE=veth-xdp ./scripts/06_compare_modes.sh                 # 对比三种模式
sudo AF_XDP_IFACE=veth-xdp ./scripts/07_collect_stats.sh                 # 收集统计
./scripts/08_make_review_bundle.sh                      # 生成报告
```

**veth pair + 流量注入方式（推荐）：**

```bash
# 创建 veth pair（如果还没有）
sudo ip link add veth-xdp type veth peer name veth-peer
sudo ip link set veth-xdp up && sudo ip link set veth-peer up
sudo ip addr add 10.99.0.2/24 dev veth-peer

# 后台运行 probe + 前台注入流量
sudo AF_XDP_IFACE=veth-xdp bash scripts/03_run_copy_mode_baseline.sh &
sleep 2
for i in $(seq 1 80); do
    ping -c 1 -W 0.02 -I veth-peer 10.99.0.1 2>/dev/null || true
done
wait
```

## 验收标准

### PASS_COPY_BASELINE

```text
COPY_BASELINE.log 中:
  PROBE_RC=0
  UMEM_READY 出现
  XSK_SOCKET_READY 出现
  AF_XDP_FINAL_STATS 出现
  程序正常退出 (bye)
```

### PASS_NATIVE_COPY

```text
NATIVE_COPY_PROBE.log 中:
  PROBE_RC=0
  XDP_ATTACHED mode=native
  XSK_SOCKET_READY 出现
```

### ZERO_COPY_PROBED

```text
ZERO_COPY_PROBE.log 存在，无论 PROBE_RC 是否为 0
只要执行了探测并记录了失败原因，就算完成
```

### PASS_TRAFFIC（附加项）

```text
AF_XDP_FINAL_STATS 中 rx_packets > 0
```

无外部流量时允许先以 PASS_COPY_BASELINE + ZERO_COPY_PROBED 通过，PASS_TRAFFIC 后续补测。

## 当前状态

- 测试日期: 2026-06-07
- 判定: **PASS_COPY_BASELINE=YES, PASS_NATIVE_COPY=YES, ZERO_COPY_PROBED=YES, PASS_RX_TRAFFIC=YES**
- 测试拓扑: veth pair (veth-peer → veth-xdp)
- COPY BASELINE (skb+copy): rx_packets=3, rx_bytes=126
- NATIVE COPY (native+copy): rx_packets=3, rx_bytes=126
- ZERO COPY (native+zero-copy): xsk_socket__create: Operation not supported (expected on veth)
- 记录: `records/20260607-140717-af-xdp-zero-copy-vs-copy/`
