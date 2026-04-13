# TASKS / stage06_arm64_migration

## M6 核心任务

- [x] 把 stage04 作为主要迁移对象，梳理 host → ARM64 的差异点
- [x] 建立 host / qemu-x86_64 / qemu-arm64 三条平台解析路径
- [x] 准备 ARM64 所需工具链、QEMU、kernel image、rootfs 参数
- [x] 为 stage04 / 后续驱动准备兼容层头文件
- [x] 输出迁移差异报告、平台矩阵、阶段报告
- [x] 在真实测试机上完成至少一轮 ARM64 build/run 验证

## 当前已落地

- [x] 平台解析脚本（resolve_platform_env.sh）
- [x] 平台矩阵生成脚本（generate_platform_matrix.sh）
- [x] stage04→stage06 差异报告骨架
- [x] ARM64 QEMU dry-run 命令生成（output/arm64_qemu_dryrun.sh）
- [x] 可复用内核兼容层头文件（include/netdev_kcompat.h）
- [x] 阶段报告生成脚本（generate_stage06_report.sh）
- [x] ARM64 模块交叉编译通过（netdev_stage04.ko，110KB aarch64 ELF）
- [x] ARM64 QEMU 实际启动并完成 smoke test

## ARM64 smoke test 验证结果（2026-04-13）

```
TX:  32 frames (RXIDX=0..31, SKBLEN=25, ETH=0x6865=0x88B7) ✅
RX:  32 POLL events (IDX=0..31, PROTO=0x88B7) ✅
NAPI poll 在 ARM64 上正常工作 ✅
端到端 smoke test 成功 ✅
```

## 关键技术修复记录

### 1. ARM64 kernel build 缺网络符号（CONFIG_NET is not set）
- **问题**：vmlinux.symvers 缺少 register_netdev/dev_add_pack/free_netdev
- **原因**：原始 kernel .config 中 # CONFIG_NET is not set
- **修复**：
  1. `.config` 中设置 `CONFIG_NET=y`、`CONFIG_NET_CORE=y`
  2. `make vmlinux` 重新链接（触发 net/core 重编 + 符号导出）
  3. `make Image` 生成新 kernel Image
  4. `make modules` 生成完整 Module.symvers

### 2. CONFIG_PACKET 缺失
- **问题**：AF_PACKET sockets 不可用，smoke 测试报错 "Address family not supported"
- **修复**：`CONFIG_PACKET=m` 加到 .config，重新 `make modules`，得到 af_packet.ko 注入 rootfs

### 3. rootfs init shebang 错误
- **问题**：`#!/busybox sh` → busybox 在 `/bin/busybox`，shebang 路径找不到
- **修复**：改为 `#!/bin/sh`（busybox 的 `/bin/sh -> busybox` 链接）

### 4. rootfs 缺少 /proc 和 /sys 目录
- **问题**：mount -t proc none /proc 失败（目录不存在）
- **修复**：在 rootfs 打包前 mkdir -p proc sys dev

## 下一步

1. 把 smoke test 集成到 `make smoke TARGET=qemu-arm64` 入口（当前需要手动跑 QEMU）
2. ARM64 rootfs 生成脚本化（整合到 Makefile）
3. 把 ARM64 smoke 结果同步回本地 records/
