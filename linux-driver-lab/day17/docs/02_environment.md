# 02_environment - 环境准备

## 1. 目录约定

Day17 默认假设你的仓库结构类似：

```text
~/workspace/driver-lab/
├── kernel-src/
│   ├── linux-5.15.10/
│   │   ├── src/
│   │   ├── build/arm64/
│   │   └── output/arm64/
│   └── busybox-1.36.1/
│       └── output/arm64/
└── linux-driver-lab/
    └── day17/
```

## 2. 需要的宿主机工具

至少需要：

- `aarch64-linux-gnu-gcc`
- `make`
- `qemu-system-aarch64`
- `dtc`
- `python3`
- `cpio`
- `file`
- `readelf`（只在集成 perf 时强烈建议有）

## 3. 推荐环境变量

```bash
export KERNEL_DIR=~/workspace/driver-lab/kernel-src/linux-5.15.10
export BUSYBOX_DIR=~/workspace/driver-lab/kernel-src/busybox-1.36.1
export CROSS_COMPILE=aarch64-linux-gnu-
```
