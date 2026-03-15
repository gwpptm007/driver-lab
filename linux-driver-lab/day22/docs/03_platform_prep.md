# day22 平台准备清单

## 1. 必备主机工具

- `qemu-system-aarch64`
- `ivshmem-server`
- `cpio`
- `gzip`
- `timeout`
- `awk`
- `grep`
- `sed`
- `file`

## 2. 你本机必须已经准备好的产物

### 2.1 内核镜像

需要一个能启动 `virt` 机器的 arm64 `Image`。

### 2.2 BusyBox

需要一个 arm64 BusyBox 二进制，最好是静态链接。

### 2.3 guest 侧 `lspci`

这是 day22 最容易卡住的点：

- guest 是 arm64
- 宿主机常常是 x86_64
- 所以不能把宿主机 `/usr/bin/lspci` 直接拷进 guest

推荐两种办法：

1. 你自己准备一个 arm64 静态 `lspci`
2. 把 `pciutils` 源码放进 `day22/third_party/pciutils/`，让 `scripts/02_build_guest_lspci.sh` 尝试交叉编译

## 3. 内核配置建议

day22 最关键的是：

- `CONFIG_PCI=y`
- `CONFIG_PCI_MSI=y`
- `CONFIG_PCI_HOST_GENERIC=y`

如果这些没打开，后面 `lspci` 大概率就没有你要的东西。

## 4. QEMU 参数原则

本 day22 里把参数拆成两层：

- 平台参数：machine / cpu / memory / kernel / initrd
- 设备参数：ivshmem socket / vectors / size

这样到 day23 时，你只改“驱动与 guest 内容”，不改 day22 的设备挂载框架。
