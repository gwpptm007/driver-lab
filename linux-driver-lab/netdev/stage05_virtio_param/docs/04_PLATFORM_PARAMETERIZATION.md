# 04. 平台参数化

从 stage05 起统一使用：

- `TARGET_ARCH`
- `RUN_MODE`
- `HOST_CC`
- `CROSS_COMPILE`
- `QEMU_BIN`
- `KERNEL_SOURCE_ROOT`
- `KERNEL_BUILD_DIR`
- `KERNEL_IMAGE`
- `ROOTFS_IMAGE`
- `VIRTIO_NET_SOURCE`

推荐组合：

- 当前：`host/host`
- stage06 重点：`arm64/qemu-arm64`
