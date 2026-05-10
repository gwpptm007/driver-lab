# 06_TROUBLESHOOTING

## 找不到 ens192

通常是前面 DPDK 实验把 PCI 设备绑定到了 `uio_pci_generic`。

检查：

```bash
sudo dpdk-devbind.py --status
lspci -nnk -s 0000:0b:00.0
```

恢复：

```bash
sudo AF_XDP_CONFIRM_REBIND=YES ./scripts/02_prepare_kernel_netdev.sh
```

## clang 找不到 asm/types.h

**错误信息**：

```
fatal error: 'asm/types.h' file not found
```

**原因**：`clang -target bpf` 不包含 x86_64 标准库路径，导致找不到 `asm/types.h`。

**解决**：在 `app/Makefile` 的 `BPF_CFLAGS` 中添加：

```
-I/usr/include/x86_64-linux-gnu
```

或者安装完整的 multiarch 交叉编译头：

```bash
sudo apt install linux-libc-dev-amd64
```

## 找不到 bpf_helpers.h / libbpf.h

安装：

```bash
sudo apt install libbpf-dev libelf-dev zlib1g-dev
```

## libbpf 太旧，bpf_xdp_attach 找不到

**错误信息**：

```
undefined reference to `bpf_xdp_attach'
undefined reference to `bpf_xdp_detach'
```

**原因**：Ubuntu 22.04 自带 libbpf 0.5.0，`bpf_xdp_attach` / `bpf_xdp_detach` 是 libbpf 1.0+ 才有的 API。

**解决**：在 `xdp_loader.c` 头部添加兼容宏（已修复，代码中已有）：

```c
#if LIBBPF_VERSION < 100
#define bpf_xdp_attach(ifindex, prog_fd, xdp_flags, _) \
    bpf_set_link_xdp_fd(ifindex, prog_fd, xdp_flags)
#define bpf_xdp_detach(ifindex, xdp_flags, _) \
    bpf_set_link_xdp_fd(ifindex, -1, xdp_flags)
#endif
```

## attach 失败 Operation not supported

先试 SKB/generic mode：

```bash
sudo AF_XDP_MODE=skb ./scripts/03_run_xdp_pass.sh
```

驱动模式：

```bash
sudo AF_XDP_MODE=drv ./scripts/03_run_xdp_pass.sh
```

VMware vmxnet3 环境下，具体是否支持 native XDP 取决于驱动和内核版本。第一站优先保证 generic/SKB mode 可跑通。

## XSKMAP / xsks_map 创建失败（BTF 相关）

**警告信息**：

```
libbpf: Error in bpf_create_map_xattr(xsks_map):ERROR: strerror_r(-524)=22(-524). Retrying without BTF.
```

**不影响功能**：这是因为内核不支持 BTF，libbpf 会自动 fallback 到非 BTF 模式，XSKMAP 仍可正常创建和使用。后续 AF_XDP socket 实验继续。

## 不要对管理网卡 DROP

脚本默认拒绝对 `ens33` 做 DROP/REDIRECT。管理网卡断开会导致 SSH 失联。
