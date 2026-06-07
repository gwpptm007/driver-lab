# AF_XDP Track Report

## 1. 背景

在完成 `netdev`、`track-dpdk` 后，AF_XDP track 用来补齐 Linux 原生用户态数据面能力。它关注的不是 DPDK PMD，而是 XDP + AF_XDP socket 如何把包从驱动早期路径导入用户态。

## 2. 已落地内容（2026-06-07 复测更新）

| 阶段 | 内容 | 状态 |
|---|---|---|
| Phase 1 | XDP redirect basics | **PASS_BASIC/ACTION/REDIRECT** (12/3/3 pkts) |
| Phase 2 | AF_XDP socket / UMEM / rings | **PASS_SOCKET/UMEM/RX_TRAFFIC** (rx=49 pkts) |
| Phase 3 | copy / zero-copy mode probe | **PASS_COPY/NATIVE/ZC_PROBED** (copy 3 pkts, ZC unsupported) |
| Phase 4 | mini forwarder | **PASS_DROP/REFLECT/TRAFFIC/TX** (tx=3, comp=3 首次) |
| Phase 5 | track summary | **UPDATED** |

## 3. 技术主线

```text
netdev XDP basics
    ↓
XDP attach / PASS / DROP / REDIRECT
    ↓
XSKMAP redirect model
    ↓
AF_XDP UMEM and rings
    ↓
copy / zero-copy support boundary
    ↓
mini forwarder project (drop + reflect)
```

## 4. 已证明

```text
BPF/XDP 程序可以编译、attach/detach
XDP_PASS / XDP_DROP / XDP_REDIRECT 三种 action 全部验证
XDP redirect → XSKMAP → AF_XDP socket → UMEM RX ring → 用户态 poll 完整数据路径
UMEM 8MB, FILL/RX/TX/COMPLETION 四环初始化与管理
FILL → RX → TX → COMPLETION → FILL 完整 frame 生命周期 (tx=3, comp=3)
skb+copy 基线, native+copy 验证, zero-copy 探测完成
veth pair 作为 XDP/AF_XDP 标准测试拓扑已验证
veth kernel 5.12+ 支持 native XDP
```

## 5. 关键突破

### veth pair 解决流量问题

同主机发包到本地 IP 走 local delivery 短路，XDP hook 不触发。改用 veth pair 从对端注入流量后，XDP hook 必定触发，四个 Phase 全部获得非零 rx_packets。

### 首次 TX/COMPLETION 验证

Phase 4 reflect 模式首次获得 tx_packets=3, comp_packets=3，完成 FILL→RX→TX→COMPLETION→FILL 完整闭环验证。这是整个 AF_XDP track 中 rx/tx/comp 三项指标首次同时非零。

### zero-copy 边界明确

veth 和 vmxnet3 均不支持 zero-copy（需要 NIC DMA），这是合理结果。skb+copy 和 native+copy 均正常工作，fallback 策略清晰。

## 6. 结论

AF_XDP track 已经完成从 XDP attach 到 mini forwarder 的完整阶段化实验。四个 Phase 全部通过复测（2026-06-07），所有数据路径均已验证。后续重点是补 traffic performance 数据和 DPDK vs AF_XDP 对比分析。
