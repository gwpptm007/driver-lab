# 05. DPDK ingress、报文解析与 mbuf 边界

ingress 的职责是把“可接收的以太网报文”缩小为“可交给 RDMA 的自包含请求”。它负责 RX、解析、分类、复制和本地入队；它不负责 QP 状态机，也不直接调用 `ibv_post_send`。

## 5.1 当前接受的输入

当前实验路径关注 Ethernet + IPv4 + UDP，UDP payload 长度必须落在 `1..GATEWAY_PAYLOAD_SIZE`（当前最大 2048 字节）。pcap 输入中每 4 个包包含 3 个 UDP 与 1 个 ICMP，因此 64 个输入包的基线结果为：

```text
rx_packets = 64
udp_packets = staged = 48
unsupported = 16
malformed = 0
```

这是一条可复现的功能性证据，不代表生产流量的协议覆盖范围。

## 5.2 解析顺序与长度检查

建议把解析看作一串必须先证明长度、再读取字段的边界检查：

```text
mbuf
  -> Ethernet: EtherType 是否 IPv4
  -> IPv4: version、IHL、total length、protocol 是否 UDP
  -> UDP: header length、UDP length 与 IPv4 可用载荷是否一致
  -> payload: 1 <= length <= 2048
```

IPv4 头长度不能假设为固定 20 字节；必须以 IHL 计算。任意一层长度短缺、互相矛盾或溢出时都记为 `malformed`，并在释放 mbuf 后结束该包处理。协议正确但当前不支持（例如 ICMP）记 `unsupported`，不要混为“解析失败”。

## 5.3 为什么使用 `rte_pktmbuf_read`

一个 mbuf 在逻辑上是一帧报文，但物理数据可分散在多个 segment。直接把 `rte_pktmbuf_mtod()` 后的内存当成整帧，可能在包头跨 segment 时越界或读到错误内容。`rte_pktmbuf_read` 允许按逻辑 offset/length 取得连续可读视图，必要时拷到调用方提供的小型临时缓冲区。

解析器因此应：

- 仅在已验证的 offset 与长度范围内调用读取；
- 不假设 Ethernet/IP/UDP 头一定都在第一个 segment；
- 只把成功校验的 UDP payload 复制到 staging slot；
- 不把临时 read buffer 指针存入 descriptor。

## 5.4 mbuf 所有权的终点

`rte_mbuf` 属于 DPDK RX/mempool 生命周期，而 RDMA 写入可能在稍后才完成。当前设计选择同步复制到 staging slot，因此所有成功和失败路径都可以在本次 ingress 处理结束时归还 mbuf：

```text
RX mbuf -> parse -> copy valid payload to staging slot -> enqueue descriptor -> free mbuf
                                  |                         |
                                  +---- failure -----------+--> free mbuf
```

因此 ring、slot、RDMA worker 中都不得保存 `rte_mbuf *` 或指向其 data 的裸指针。任何“零拷贝”重构都必须先解决 mbuf 外部内存注册、引用计数、DMA 可见性和 CQE 前释放问题；不能仅删除 memcpy。

## 5.5 入队前的提交顺序

一个 UDP 请求在 producer 侧必须按以下顺序处理：

1. 获取 FREE slot，并分配新 generation；
2. 复制并记录 payload 长度；
3. 填写 `gateway_request`（包括 ingress port、RX queue、flow hash）；
4. release 发布到 SPSC ring；
5. 无论成功或失败，释放当前 mbuf。

如果第 4 步因 ring 满失败，应取消 READY slot；如果第 1 步无可用 slot，则不得写入任何 staging payload。这样不会出现“环中 descriptor 指向半准备的 slot”。

## 5.6 统计是协议边界的一部分

当前 ingress 统计至少分为：

| 计数 | 含义 |
| --- | --- |
| `rx_packets` | 从 RX burst 取到的报文 |
| `udp_packets` | 通过协议分类的 UDP 报文 |
| `unsupported` | 格式可读但不在当前协议范围 |
| `malformed` | 头部/长度不合法或不完整 |
| `staged` | 已成功复制并入队的请求 |
| `ring_full` | 准备后无法发布，已回退 slot |
| `slot_exhausted` | 无 FREE slot，未开始写 payload |

在当前基线中，`rx_packets = udp_packets + unsupported + malformed`，且无拥塞时 `udp_packets = staged`。这些等式是测试断言，不是仅供展示的日志。

## 5.7 扩展协议前先写策略

支持 VLAN、IPv6、隧道、分片或硬件 checksum offload 前，必须先决定：每种报文是接受、拒绝还是在软件重组；flow hash 包含哪些字段；VLAN/隧道元数据放入 ABI 的何处；以及计数如何区分。特别是 IPv4 分片不能把“第一片有 UDP 头”误当作“一整个 UDP datagram 已可写出”。

相关代码：[`gateway_ingress.h`](../../include/gateway_ingress.h)、[`gateway_ingress.c`](../../src/gateway_ingress.c)、[`test_gateway_ingress.c`](../../tests/test_gateway_ingress.c)。
