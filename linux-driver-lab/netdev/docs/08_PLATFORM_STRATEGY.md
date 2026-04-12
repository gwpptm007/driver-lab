# 08. 平台策略

## 一、当前正式策略

### Stage01~Stage04
专注 netdev 本体，不被 ARM64 工具链干扰。

### Stage05~Stage06
再做一次 ARM64 迁移，作为独立技能点和工程化收口。

## 二、为什么这么切

### 1. 先学驱动语义，再学平台迁移
这样更符合认知规律，也更利于阶段验收。

### 2. 避免早期被环境问题吞噬
交叉编译、QEMU 架构、BusyBox/rootfs、宿主机网络联通，都会显著增加早期摩擦。

### 3. 后期迁移更能体现项目价值
当主线已经立住，再做 ARM64 迁移，会多出一个很清晰的成果：

- 平台可配置
- 迁移可复现
- 差异可解释

## 三、平台可配置原则

从 Stage00 开始就避免写死：

- `ARCH=arm64`
- `qemu-system-aarch64`
- `CROSS_COMPILE=aarch64-linux-gnu-`
- 某个固定 `Image`
- 某个固定 BusyBox 路径

应优先使用变量：

- `TARGET_ARCH`
- `RUN_MODE`
- `QEMU_BIN`
- `KERNEL_IMAGE`
- `ROOTFS_IMAGE`
- `CROSS_COMPILE`
- `HOST_CC`

## 四、Stage05 与 Stage06 的职责切分

### Stage05
- `virtio-net` 对照
- 平台参数化
- 差异清单准备

### Stage06
- ARM64 实迁
- 差异验证
- 跨平台回归与最终报告
