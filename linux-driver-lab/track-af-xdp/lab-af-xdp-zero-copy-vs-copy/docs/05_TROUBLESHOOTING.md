# 05_TROUBLESHOOTING

## xsk_socket__create: Operation not supported

通常说明驱动不支持当前 AF_XDP bind mode，尤其是 zero-copy。

处理：

```bash
AF_XDP_BIND_MODE=copy AF_XDP_MODE=skb sudo ./scripts/03_run_copy_mode_baseline.sh
```

## bpf_xdp_attach failed

可能原因：

- native mode 不受驱动支持；
- 接口已有 XDP 程序；
- 权限不足；
- 网卡还绑在 DPDK uio/vfio 驱动。

处理：

```bash
sudo ./scripts/09_clean_runtime.sh
sudo AF_XDP_CONFIRM_REBIND=YES ./scripts/02_prepare_kernel_netdev.sh
```

## 统计为 0

本实验不是 traffic lab。统计为 0 不影响模式探测。需要真实 RX 时，后续 `project-af-xdp-mini-forwarder` 再做完整流量闭环。
