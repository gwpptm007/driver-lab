# day22 START_HERE

## 先记住一句话

**这次的 day22 不再只是平台准备，而是“设备可见性 + C 代码起步”一起做。**

你今天至少会接触到两份真正的代码：

- `tools/pci_sysfs_dump.c`
- `driver/day22_ivshmem_stub.c`

## 建议按这个顺序走

1. 先读 `README.md`
2. 读 `tools/pci_sysfs_dump.c`
3. 读 `driver/day22_ivshmem_stub.c`
4. 配置 `env/day22.env` 或导出环境变量
5. 运行 `make check`
6. 运行 `make build-tools`
7. 运行 `make rootfs`
8. 运行 `make run`
9. 去 `records/<run-id>/` 检查结果
10. 有内核构建目录时，再运行 `make module`

## 最少需要准备的三个路径

```bash
export KERNEL_IMAGE=/path/to/arch/arm64/boot/Image
export BUSYBOX_BIN=/path/to/busybox
export GUEST_LSPCI_BIN=/path/to/aarch64-static-lspci
```

## 如果你想把 stub 模块也编出来

```bash
export KDIR=/path/to/kernel/build
make module
```

## 今天别做过头的点

- 不要抢跑去把 BAR/MMIO/MSI 一次写完
- 不要把 `shared-memory` 误写成 `DMA`
- 不要只看屏幕输出，不归档原始证据
- 不要忽略 `tools/` 和 `driver/` 里的 C 代码
