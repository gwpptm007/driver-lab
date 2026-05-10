# 04_ACCEPTANCE

## PASS_SOCKET_READY

满足：

- `XSK_SOCKET_READY` 出现；
- `XSKMAP_REGISTERED` 出现；
- 程序正常退出 `bye`。

## PASS_UMEM_RINGS

满足：

- `UMEM_READY` 出现；
- `FILL_RING_READY` 出现；
- `AF_XDP_RINGS_READY` 出现。

## PASS_RX_TRAFFIC

满足：

- `AF_XDP_FINAL_STATS` 出现；
- `rx_packets > 0`。

## 当前可接受状态

如果暂时没有外部发包源，本阶段允许先以：

```text
PASS_SOCKET_READY + PASS_UMEM_RINGS
```

作为第二站 smoke 通过标准。`PASS_RX_TRAFFIC` 后续补测。
