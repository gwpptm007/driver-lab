# AF_XDP Backlog

## P0: 补测试 records

```text
lab-xdp-redirect-basics:
  XDP_DROP.log
  XDP_REDIRECT_DRYRUN.log
  非 0 action stats

lab-af-xdp-socket-rings:
  UMEM_READY
  XSK_SOCKET_READY
  FILL_RING_READY
  XSKMAP_REGISTERED
  AF_XDP_FINAL_STATS

lab-af-xdp-zero-copy-vs-copy:
  PASS_COPY_BASELINE
  ZERO_COPY_PROBED
  PASS_ZERO_COPY 或 NOT_SUPPORTED_ON_THIS_ENV

project-af-xdp-mini-forwarder:
  PASS_DROP_SMOKE
  PASS_REFLECT_SMOKE
  PASS_TRAFFIC / PASS_TX_REFLECT 后续补
```

## P1: veth/namespace 测试拓扑

建议后续增加不依赖外部机器的 veth namespace 拓扑：

```text
ns-tx veth0
    ↓
veth1 + XDP/AF_XDP
    ↓
mini forwarder
```

这样可以更稳定地产生 RX 统计。

## P2: 和 DPDK/DPDK media-gateway-lite 对比

后续可以做一份 `DPDK vs AF_XDP` 对比文档：

```text
PMD vs kernel driver
mbuf vs UMEM frame
rx_burst/tx_burst vs rings
hugepage vs mmap UMEM
vhost/virtio-user vs XSKMAP redirect
```

## P3: 性能与可观测性

等功能 records 完整后，再补：

```text
pps baseline
CPU usage
drop reason
ring occupancy
busy poll / need_wakeup
```
