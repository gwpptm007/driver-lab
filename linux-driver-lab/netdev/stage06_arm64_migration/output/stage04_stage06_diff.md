# stage04 -> stage06 diff

## 迁移对象

- 源阶段：stage04_ring_dma
- 目标阶段：stage06_arm64_migration

## northbound 保持不变

- net_device 视角
- ndo_start_xmit / NAPI / stats 的观察方法
- debugfs / smoke / records 的组织方式

## southbound 发生变化

| 维度 | stage04 默认 | stage06 目标 |
|---|---|---|
| arch | host / 未强绑 | arm64 为重点 |
| build | native gcc + host KDIR | cross toolchain + arm64 build dir |
| run | host 侧调试为主 | qemu-arm64 dry-run / 真机运行 |
| kernel image | 非必须 | ARM64 Image 必要 |
| rootfs | 非必须 | ARM64 rootfs / initrd 必要 |

## 当前解析到的 ARM64 关键参数

- QEMU_BIN: /usr/bin/qemu-system-aarch64
- CROSS_COMPILE: aarch64-linux-gnu-
- KDIR / KERNEL_BUILD_DIR: /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/build/arm64
- KERNEL_IMAGE: /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/output/arm64/Image
- ROOTFS_IMAGE: /home/wq7/workspace/driver-lab/linux-driver-lab/netdev/stage06_arm64_migration/output/rootfs.img

## 推荐迁移清单

1. 先保证 stage04 可以在 arm64 build tree 上编译
2. 再生成 qemu-system-aarch64 命令行
3. 再做真正运行验证
4. 最后输出 ARM64 smoke 记录
