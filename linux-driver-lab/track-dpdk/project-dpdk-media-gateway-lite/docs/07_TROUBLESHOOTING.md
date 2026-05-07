# 07_TROUBLESHOOTING

## 只有一个端口时 tx=0

单 vmxnet3 口场景下，程序只能做 RX/classify/no-route drop，`tx=0` 是正常的。

要验证 forwarding，需要：

- 两个 DPDK 物理/虚拟端口；或
- vhost/virtio-user 拓扑；或
- `net_null0/net_null1` smoke。

## rx 一直是 0

常见原因：

1. 没有外部发包源；
2. 目的 MAC 不对；
3. VMware 网络模式不通；
4. `ens192` 没有绑定到 DPDK driver；
5. 发包在同一个被 DPDK 接管的 Linux 接口上执行。

## rewrite 是 0

需要同时满足：

1. 有 IPv4/UDP 流量；
2. 规则方向匹配；
3. 可选 match 条件匹配；
4. rewrite 字段被配置。
