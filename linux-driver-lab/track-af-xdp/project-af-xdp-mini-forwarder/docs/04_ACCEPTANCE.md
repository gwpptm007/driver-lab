# 04_ACCEPTANCE

## PASS_BUILD

需要：

```text
BUILD_RESULT=PASS
build/af_xdp_forwarder
build/af_xdp_forwarder_kern.bpf.o
```

## PASS_SOCKET_READY

需要日志包含：

```text
UMEM_READY
XSK_SOCKET_READY
FILL_RING_READY
XDP_ATTACHED
XSKMAP_REGISTERED
AF_XDP_FORWARDER_READY
```

## PASS_DROP_SMOKE

需要：

```text
AF_XDP_FORWARDER_READY mode=drop
FORWARDER_FINAL_STATS
bye
```

## PASS_REFLECT_SMOKE

需要：

```text
AF_XDP_FORWARDER_READY mode=reflect
FORWARDER_FINAL_STATS
bye
```

如果没有真实流量，`rx_packets=0` 可以接受，但只能算 smoke。

## PASS_TRAFFIC

需要：

```text
rx_packets > 0
rx_bytes > 0
```

## PASS_TX_REFLECT

需要：

```text
tx_packets > 0
comp_packets > 0
```

注意：VMware/vmxnet3/AF_XDP copy path 下是否能稳定 TX，需要以测试机实际记录为准。
