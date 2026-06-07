# 03_SETUP_AND_TROUBLESHOOTING — 环境搭建与排错

## 编译依赖

```bash
sudo apt install clang llvm make pkg-config
sudo apt install libbpf-dev libelf-dev zlib1g-dev
sudo apt install linux-headers-$(uname -r)
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

DPDK 需要网卡绑定到 `uio_pci_generic/vfio-pci`，XDP/AF_XDP 需要回到 `vmxnet3`：

```bash
sudo dpdk-devbind.py --status
lspci -nnk -s 0000:0b:00.0
# 恢复：
sudo modprobe vmxnet3
sudo dpdk-devbind.py -b vmxnet3 0000:0b:00.0
```

## 常见问题

### 1. xsk_socket__create: Operation not supported

通常说明驱动不支持当前 AF_XDP bind mode，尤其是 zero-copy。在 veth/vmxnet3 环境下 zero-copy 失败是**预期行为**，不是实验失败。

处理：fallback 到 copy mode。

```bash
AF_XDP_BIND_MODE=copy AF_XDP_MODE=skb sudo ./scripts/03_run_copy_mode_baseline.sh
```

### 2. bpf_xdp_attach failed

可能原因：
- native mode 不受驱动支持
- 接口已有 XDP 程序
- 权限不足
- 网卡还绑在 DPDK uio/vfio 驱动

处理：

```bash
sudo ./scripts/09_clean_runtime.sh
sudo AF_XDP_CONFIRM_REBIND=YES ./scripts/02_prepare_kernel_netdev.sh
```

### 3. libbpf 版本兼容

Ubuntu 22.04 自带 libbpf 0.5.0，`bpf_xdp_attach()` 是 libbpf 1.0+ API。代码已加兼容宏：

```c
#define bpf_xdp_attach(ifindex, prog_fd, xdp_flags, _) \
    bpf_set_link_xdp_fd(ifindex, prog_fd, xdp_flags)
```

### 4. XSKMAP 创建 BTF 警告

```
libbpf: Error in bpf_create_map_xattr(xsks_map):ERROR: strerror_r(-524)=22(-524). Retrying without BTF.
```

不影响功能，libbpf 自动 fallback 到非 BTF 模式。

### 5. rx_packets 一直是 0

这是最常见的"不是 bug 的 bug"：

- **原因：** 同主机发包到本地 IP 走 local delivery 短路，XDP hook 不触发
- **解决：** 用 veth pair 替代物理网卡，从对端注入流量
- **详细说明：** 见 [04_RETEST_20260607.md](04_RETEST_20260607.md) 和 Phase 1 的 [05_VETH_PAIR_DEEP_DIVE.md](../lab-xdp-redirect-basics/docs/05_VETH_PAIR_DEEP_DIVE.md)

### 6. 不要对管理网卡操作

脚本拒绝对 `ens33` 做任何 XDP 操作，防止 SSH 断开。
