# 02_TEST_MACHINE_ENV

## 编译依赖（必须安装）

```bash
# 编译工具链
sudo apt install clang llvm make pkg-config

# libbpf 开发库
sudo apt install libbpf-dev libelf-dev zlib1g-dev

# 内核头文件（解决 asm/types.h not found）
sudo apt install linux-headers-$(uname -r)

# bpftool（通常已有）
sudo apt install bpftool
```

> **已知问题**：Ubuntu 22.04 的 `clang -target bpf` 编译时，默认 include 路径不包含 `/usr/include/x86_64-linux-gnu/`，导致找不到 `asm/types.h`。解决方案见 `app/Makefile` 中的 `BPF_CFLAGS += -I/usr/include/x86_64-linux-gnu`。

## 测试机环境

```
OS：Ubuntu 22.04.5 LTS
Kernel：6.8.0-110-generic
Hypervisor：VMware
管理网卡：ens33 / e1000
测试网卡：ens192 / vmxnet3 / 0000:0b:00.0
```

## 重要区别

DPDK 实验需要把测试网卡绑定给：

```
uio_pci_generic / vfio-pci
```

XDP / AF_XDP 实验需要测试网卡回到内核网络驱动：

```
vmxnet3
```

所以如果 `ens192` 消失，先检查：

```bash
sudo dpdk-devbind.py --status
lspci -nnk -s 0000:0b:00.0
```

恢复示例：

```bash
sudo modprobe vmxnet3
sudo dpdk-devbind.py -b vmxnet3 0000:0b:00.0
```

本 lab 提供：

```bash
sudo AF_XDP_CONFIRM_REBIND=YES ./scripts/02_prepare_kernel_netdev.sh
```

脚本默认不会操作管理网卡 `ens33`。
