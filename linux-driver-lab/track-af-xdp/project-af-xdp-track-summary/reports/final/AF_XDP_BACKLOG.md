# AF_XDP Backlog

> 2026-06-07 更新：P0 全部完成，四个 Phase 均已复测通过。

## P0: 补测试 records — DONE

```text
lab-xdp-redirect-basics:
  XDP_PASS.log — DONE (12 pkts)
  XDP_DROP.log — DONE (3 pkts)
  XDP_REDIRECT_DRYRUN.log — DONE (3 pkts)

lab-af-xdp-socket-rings:
  UMEM_READY — DONE
  XSK_SOCKET_READY — DONE
  FILL_RING_READY — DONE
  XSKMAP_REGISTERED — DONE
  AF_XDP_FINAL_STATS rx_packets=49 — DONE

lab-af-xdp-zero-copy-vs-copy:
  PASS_COPY_BASELINE — DONE (rx=3 pkts)
  PASS_NATIVE_COPY — DONE (rx=3 pkts)
  ZERO_COPY_PROBED=YES, PASS_ZERO_COPY=NO — DONE (veth no DMA)

project-af-xdp-mini-forwarder:
  PASS_DROP_SMOKE — DONE (rx=3, drop=3)
  PASS_REFLECT_SMOKE — DONE (rx=3, tx=3, comp=3 首次!)
  PASS_TRAFFIC=YES — DONE
  PASS_TX_REFLECT=YES — DONE
```

## P1: veth/namespace 测试拓扑

已在所有 Phase 中验证 veth pair 作为测试拓扑。后续可扩展：

```text
ns-tx (namespace)
    │
 veth-tx
    │
 veth-rx + XDP/AF_XDP
    │
 mini forwarder
```

用 namespace 替代裸 veth pair，可以更稳定地产生流量和控制变量。

## P2: project-af-xdp-traffic-test

给 mini forwarder 补完整流量测试：

- UDP flood (不同包大小: 64/512/1500)
- pps 基线 (skb+copy)
- ring occupancy 监控
- busy poll / need_wakeup 性能对比
- drop rate vs pps 曲线

## P3: DPDK vs AF_XDP 对比

```text
PMD vs kernel driver
mbuf vs UMEM frame
rx_burst/tx_burst vs rings
hugepage vs mmap UMEM
vhost/virtio-user vs XSKMAP redirect
CPU usage / cache miss 对比
```

## P4: 物理网卡验证

当前所有测试使用 veth pair。后续需要在真实物理网卡上复测：

- ixgbe/i40e 物理网卡上的 native XDP + zero-copy
- 对比物理网卡 vs veth 的性能差异
