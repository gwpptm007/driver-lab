# 02_SETUP_AND_TROUBLESHOOTING — 环境搭建与排错

## 编译依赖

```bash
# 编译工具链
sudo apt install clang llvm make pkg-config

# libbpf 开发库
sudo apt install libbpf-dev libelf-dev zlib1g-dev

# 内核头文件（解决 asm/types.h not found）
sudo apt install linux-headers-$(uname -r)

# bpftool
sudo apt install bpftool
```

## 测试机环境

```
OS: Ubuntu 22.04.5 LTS
Kernel: 6.8.0-111-generic
Hypervisor: VMware
管理网卡: ens33 / e1000 / 192.168.65.135
测试网卡: ens192 / vmxnet3 / 0000:0b:00.0 / 192.168.100.1
```

## DPDK vs XDP 网卡绑定

DPDK 实验需要测试网卡绑定到 `uio_pci_generic` / `vfio-pci`，XDP/AF_XDP 需要回到内核驱动 `vmxnet3`。

检查当前绑定：

```bash
sudo dpdk-devbind.py --status
lspci -nnk -s 0000:0b:00.0
```

恢复内核驱动：

```bash
sudo modprobe vmxnet3
sudo dpdk-devbind.py -b vmxnet3 0000:0b:00.0
# 或使用本 lab 脚本：
sudo AF_XDP_CONFIRM_REBIND=YES ./scripts/02_prepare_kernel_netdev.sh
```

脚本默认拒绝操作管理网卡 `ens33`。

## 常见问题

### 1. clang 找不到 asm/types.h

**错误：** `fatal error: 'asm/types.h' file not found`

**原因：** `clang -target bpf` 默认不包含 x86_64 标准库路径。

**解决：** `app/Makefile` 中 `BPF_CFLAGS` 已添加 `-I/usr/include/x86_64-linux-gnu`。或安装：

```bash
sudo apt install linux-libc-dev-amd64
```

### 2. 找不到 bpf_helpers.h / libbpf.h

```bash
sudo apt install libbpf-dev libelf-dev zlib1g-dev
```

### 3. libbpf 太旧，bpf_xdp_attach 找不到

**错误：** `undefined reference to 'bpf_xdp_attach'`

**原因：** Ubuntu 22.04 自带 libbpf 0.5.0，`bpf_xdp_attach` / `bpf_xdp_detach` 是 libbpf 1.0+ API。

**解决：** `xdp_loader.c` 中已有兼容宏：

```c
#define bpf_xdp_attach(ifindex, prog_fd, xdp_flags, _) \
    bpf_set_link_xdp_fd(ifindex, prog_fd, xdp_flags)
#define bpf_xdp_detach(ifindex, xdp_flags, _) \
    bpf_set_link_xdp_fd(ifindex, -1, xdp_flags)
```

### 4. attach 失败 Operation not supported

先试 SKB (generic) mode：

```bash
sudo AF_XDP_MODE=skb bash scripts/03_run_xdp_pass.sh
```

vmxnet3 环境下 native XDP 可能不被支持，SKB mode 始终可用。

### 5. XSKMAP 创建时的 BTF 警告

**警告：** `libbpf: Error in bpf_create_map_xattr(xsks_map):ERROR: strerror_r(-524)=22(-524). Retrying without BTF.`

**不影响功能。** 内核不支持 BTF 时 libbpf 自动 fallback，XSKMAP 正常创建。

### 6. ens192 找不到

通常是 DPDK 实验把 PCI 设备绑走了。检查并恢复（见上方"DPDK vs XDP 网卡绑定"）。

### 7. 不要对管理网卡操作

脚本默认拒绝对 `ens33` 做 DROP/REDIRECT，防止 SSH 断开。

### 8. XDP 测试 packets=0

见 [04_RETEST_20260607.md](04_RETEST_20260607.md) 和 [05_VETH_PAIR_DEEP_DIVE.md](05_VETH_PAIR_DEEP_DIVE.md)。根本原因：同主机发包到本地 IP 走 local delivery 短路，XDP hook 不触发。解决方案：用 veth pair。
