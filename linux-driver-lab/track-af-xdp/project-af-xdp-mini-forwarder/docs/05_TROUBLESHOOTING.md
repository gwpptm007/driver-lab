# 05_TROUBLESHOOTING

## xsk_socket__create 失败

常见原因：

- 接口没有回到内核驱动；
- 队列号不对；
- 权限不足；
- libbpf/xsk 版本差异；
- zero-copy/native 模式不被驱动支持。

先用默认：

```bash
AF_XDP_MODE=skb AF_XDP_BIND=copy
```

## XDP attach 失败

检查是否已有 XDP 程序：

```bash
ip -details link show ens192
```

清理：

```bash
sudo ./scripts/08_clean_runtime.sh
```

## rx_packets 一直为 0

这通常不是程序失败，而是接口没有流量。需要：

- 外部 VM/宿主机向 `ens192` 发包；
- 或让同网段产生 ARP/ping；
- 或后续做 veth/namespace 专用流量测试。

## reflect 模式 tx 仍为 0

如果 RX 为 0，则 TX 必然为 0。先让 RX 非 0。

如果 RX 非 0 但 TX 为 0，检查：

- TX ring reserve 是否失败；
- completion ring 是否有返回；
- 驱动是否支持当前 AF_XDP TX 路径。
