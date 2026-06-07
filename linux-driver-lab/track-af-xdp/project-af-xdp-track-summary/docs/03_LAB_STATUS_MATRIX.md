# AF_XDP Lab Status Matrix

## 复测日期：2026-06-07

所有四个 Phase 均使用 veth pair 拓扑（veth-peer → veth-xdp）完成复测，解决了同主机发包 XDP hook 不触发的问题。

| 阶段 | 状态 | 测试拓扑 | 关键数据 |
|---|---|---|---|
| `lab-xdp-redirect-basics` | **PASS_BASIC/ACTION/REDIRECT** | veth-xdp | PASS: 12 pkts, DROP: 3 pkts, REDIRECT: 3 pkts |
| `lab-af-xdp-socket-rings` | **PASS_SOCKET/UMEM/RX_TRAFFIC** | veth-xdp | rx_packets=49 (首轮), UMEM 8MB, FILL/RX/TX/COMP rings |
| `lab-af-xdp-zero-copy-vs-copy` | **PASS_COPY/NATIVE/ZC_PROBED** | veth-xdp | skb+copy: 3 pkts, native+copy: 3 pkts, ZC: unsupported |
| `project-af-xdp-mini-forwarder` | **PASS_DROP/REFLECT/TRAFFIC/TX** | veth-xdp | DROP: rx=3/drop=3, REFLECT: rx=3/tx=3/comp=3 |
| `project-af-xdp-track-summary` | **READY** | — | 总报告、面试材料、backlog 已就绪 |

## 各 Phase 详细判定

### Phase 1: lab-xdp-redirect-basics

```text
PASS_BASIC=YES          BUILD + XDP attach/detach + stats
PASS_ACTION=YES         XDP_DROP 验证 action 控制
REDIRECT_MODEL_READY=YES XSKMAP redirect dry-run 通过
```

记录：`records/20260607-132613-xdp-redirect-basics/`

### Phase 2: lab-af-xdp-socket-rings

```text
PASS_SOCKET_READY=YES   UMEM + XSK socket + XSKMAP 注册
PASS_UMEM_RINGS=YES     UMEM 8MB, FILL/RX/TX/COMP rings 初始化
PASS_RX_TRAFFIC=YES     rx_packets=49 (首轮 6663 bytes)
```

记录：`records/20260607-135550-af-xdp-socket-rings/`

### Phase 3: lab-af-xdp-zero-copy-vs-copy

```text
PASS_COPY_BASELINE=YES  skb+copy 基线: rx_packets=3
PASS_NATIVE_COPY=YES    native+copy 在 veth 上通过: rx_packets=3
ZERO_COPY_PROBED=YES    native+zero-copy 已探测
PASS_ZERO_COPY=NO       veth 无 DMA，不支持 ZC（预期结果）
```

记录：`records/20260607-140717-af-xdp-zero-copy-vs-copy/`

### Phase 4: project-af-xdp-mini-forwarder

```text
PASS_BUILD=YES          编译通过
PASS_DROP_SMOKE=YES     drop 模式稳定运行
PASS_REFLECT_SMOKE=YES  reflect 模式稳定运行
PASS_TRAFFIC=YES        rx_packets=3 (drop), rx_packets=3 (reflect)
PASS_TX_REFLECT=YES     tx_packets=3, comp_packets=3 — 首次验证 TX+COMPLETION
```

记录：`records/20260607-140717-af-xdp-mini-forwarder/`

## 关键突破

1. **veth pair 解决了所有 Phase 的流量问题**：同主机发包到本地 IP 走 local delivery 短路，XDP hook 不会触发。改用 veth pair 从对端注入流量后，XDP hook 必定触发。

2. **veth 支持 native XDP**：kernel 5.12+ 的 veth 驱动实现了 native XDP，veth 上的 native+copy 模式完全正常。

3. **首个 TX/COMPLETION 验证**：Phase 4 reflect 模式首次获得 tx_packets=3, comp_packets=3，验证了 FILL → RX → TX → COMPLETION → FILL 的完整 frame 生命周期闭环。
