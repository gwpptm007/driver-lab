# START_HERE

## 第一步：检查环境

```bash
./scripts/00_check_env.sh
```

重点看：

- `AF_XDP_IFACE=ens192`；
- `AF_XDP_MANAGEMENT_IFACE=ens33`；
- `libbpf` / `clang` / `bpftool` 是否可用；
- `ens192` 是否已经回到 `vmxnet3` 内核驱动。

## 第二步：编译

```bash
./scripts/01_build_app.sh
```

成功标志：

```text
BUILD_RESULT=PASS
build/af_xdp_forwarder
build/af_xdp_forwarder_kern.bpf.o
```

## 第三步：准备网卡

```bash
sudo AF_XDP_CONFIRM_REBIND=YES ./scripts/02_prepare_kernel_netdev.sh
```

注意：脚本拒绝操作管理网卡 `ens33`。

## 第四步：drop smoke

```bash
sudo ./scripts/03_run_forwarder_drop_smoke.sh
```

成功标志：

```text
UMEM_READY
XSK_SOCKET_READY
FILL_RING_READY
XDP_ATTACHED
XSKMAP_REGISTERED
AF_XDP_FORWARDER_READY
FORWARDER_FINAL_STATS
bye
```

## 第五步：reflect smoke

```bash
sudo ./scripts/04_run_forwarder_reflect_smoke.sh
```

如果没有外部流量，`rx_packets` 可能是 0，这不代表程序失败。真实流量后续补测。
