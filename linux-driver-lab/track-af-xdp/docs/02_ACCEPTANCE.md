# 02_ACCEPTANCE

## lab-xdp-redirect-basics

| Verdict | Criteria |
|---|---|
| PASS_BASIC_ATTACH | build ok, XDP attach/detach ok |
| PASS_ACTION | PASS/DROP logs exist and action stats are observable |
| REDIRECT_MODEL_READY | redirect dry-run log exists |

当前：`PASS_BASIC_ATTACH`，后续补测 ACTION/REDIRECT。

## lab-af-xdp-socket-rings

| Verdict | Criteria |
|---|---|
| PASS_SOCKET_READY | `XSK_SOCKET_READY` + `XSKMAP_REGISTERED` |
| PASS_UMEM_RINGS | `UMEM_READY` + `FILL_RING_READY` + `AF_XDP_RINGS_READY` |
| PASS_RX_TRAFFIC | `AF_XDP_FINAL_STATS rx_packets > 0` |

## lab-af-xdp-zero-copy-vs-copy

计划：

- `skb+copy` 可跑；
- `native` 支持性明确；
- `zero-copy` 支持性明确；
- records 中说明网卡/驱动限制。

## project-af-xdp-mini-forwarder

计划：

- 至少两个 AF_XDP socket 或一收一发模型；
- RX/TX 统计；
- drop reason；
- review bundle。
