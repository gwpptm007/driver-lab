# platform_matrix

| profile | arch | run mode | qemu | cross toolchain | kernel build dir | kernel image | rootfs |
|---|---|---|---|---|---|---|---|
| host | host | host | n/a | native gcc | /lib/modules/6.8.0-107-generic/build | n/a | n/a |
| qemu-x86_64 | x86_64 | qemu-x86_64 | /usr/bin/qemu-system-x86_64 | native gcc | n/a | n/a | n/a |
| qemu-arm64 | arm64 | qemu-arm64 | /usr/bin/qemu-system-aarch64 | aarch64-linux-gnu- | /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/build/arm64 | /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/output/arm64/Image | stage06/output/rootfs.img |

## host 能力摘要

- qemu-system-x86_64 available: yes
- qemu-system-aarch64 available: yes
- aarch64-linux-gnu-gcc available: yes

## ARM64 构建结果

- ARM64 cross-compile: ✅ 源码编译成功（flags/include 路径均正确）
- MODPOST: ❌ ARM64 kernel build 缺少 vmlinux.symvers（register_netdev 等核心符号未导出）
- 阻塞原因：kernel build 配置问题，非驱动代码问题

## 说明

- host 行只代表宿主环境原生构建/检查能力
- qemu-x86_64 行主要用于 run 方式与参数整理
- qemu-arm64 行是 stage06 的最终重点
