# 05_TROUBLESHOOTING

## rx 一直是 0

常见原因：

1. 外部发包没有打到 DPDK 口所在网络；
2. 目的 MAC 不对；
3. VMware 网络模式不通；
4. DPDK 口没有成功绑定到 `uio_pci_generic`；
5. 发包是在同一 guest 的 kernel `ens192` 上做的，但 `ens192` 已经被 DPDK 接管。

## tx 一直是 0

单端口 `RX/classify/free` 模式下 tx=0 是正常的。只有双端口或虚拟双端口模式下才要求 tx 非 0。

## rewrite 是 0

需要同时满足：

1. `--rewrite 1`；
2. 流量是 IPv4/UDP；
3. rewrite 匹配条件和报文匹配；
4. 程序没有因为 `udp_only` 或短包提前 drop。
