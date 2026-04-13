# stage06_arm64_migration report

## 阶段定义

- 当前阶段：ARM64 迁移与跨平台收口
- 主迁移对象：stage04_ring_dma
- 当前定位：先让 build/run/records/compat 都能落到 ARM64

## host 能力（wq7 测试机）

- Host kernel: 6.8.0-107-generic
- qemu-system-x86_64 available: yes
- qemu-system-aarch64 available: yes
- aarch64-linux-gnu-gcc available: yes

## ARM64 解析结果

- QEMU_BIN: /usr/bin/qemu-system-aarch64
- CROSS_COMPILE: aarch64-linux-gnu-
- KDIR: /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/build/arm64
- KERNEL_IMAGE: /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/output/arm64/Image
- ROOTFS_IMAGE: stage06/output/rootfs_arm64.img (含 smoke test)

## 当前可执行性判断

- STAGE06_ARM64_READY=yes

## ARM64 smoke test 结果（2026-04-13）

```
TX:  32 frames (RXIDX=0..31, SKBLEN=25, ETH=0x6865=0x88B7) ✅
RX:  32 POLL events (IDX=0..31, PROTO=0x88B7) ✅
NAPI poll 在 ARM64 上正常工作 ✅
端到端 smoke test 成功 ✅
```

smoke 验证路径：QEMU ARM64 → busybox rootfs → insmod af_packet.ko → insmod netdev_stage04.ko → send/recv burst

## 本阶段交付

- 平台矩阵：`output/platform_matrix.md`
- 迁移差异报告：`output/stage04_stage06_diff.md`
- ARM64 dry-run 命令：`output/arm64_qemu_dryrun.sh`
- 兼容层代码：`include/netdev_kcompat.h`（含 5.15.x u64_stats 兼容）
- ARM64 smoke 记录：`records/20260413-arm64-smoke/dmesg.txt`

## 关键技术修复

1. `resolve_platform_env.sh`：增加 ARM64 fallback 路径和 `source stage env`
2. `build_stage04_for_target.sh`：传入 `ARCH=${TARGET_ARCH}`
3. `stage04 Makefile`：`build-module` 传入 `ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE)`
4. `netdev_stage04.c`：`u64_stats` 宏增加 `LINUX_VERSION_CODE >= KERNEL_VERSION(5,15,0)` 分支
5. `netdev_kcompat.h`：同步修复 `NETDEV_KCOMPAT_U64_*` 宏
6. **ARM64 kernel build**：`CONFIG_NET=y`、`CONFIG_NET_CORE=y`（修复 vmlinux.symvers 缺符号）
7. **ARM64 kernel build**：`CONFIG_PACKET=m`（启用 AF_PACKET sockets）
8. **rootfs init**：`#!/busybox sh` → `#!/bin/sh`（busybox shebang 路径修复）
9. **rootfs 目录**：`mkdir -p /proc /sys /dev`（busybox rootfs 初始化）

## 下一步

- smoke test 集成到 `make smoke TARGET=qemu-arm64` 入口
- ARM64 rootfs 生成脚本化
