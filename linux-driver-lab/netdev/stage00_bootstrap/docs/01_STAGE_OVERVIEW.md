# STAGE_OVERVIEW

## stage00 是什么

stage00_bootstrap 是 netdev 主线的第零步，**不写驱动代码，只做环境验证**。

## 核心目标

1. **架构中立**：不默认 ARM64，默认 `TARGET_ARCH=host`
2. **平台可参数化**：工具链和路径通过变量注入，不写死
3. **环境验证**：进入 stage01 之前，确认工具链可用
4. **可复现**：在不同机器上能跑出相同结果

## 为什么不用 ARM64

ARM64 交叉编译复杂度高，容易把环境问题和学习问题混在一起。stage00~04 全程用 x86 先把 netdev 概念学透，stage06 再做 ARM64 迁移。

## 做了什么

```
make all
  ├─ discover_paths.sh     → 自动扫描 kernel/busybox/QEMU 路径
  ├─ check_host_tools.sh  → 检查 gcc/make/qemu/ip/ethtool/perf
  └─ generate_report.sh   → 输出 stage00_report.md
```

## 怎么判断过了

`output/stage00_report.md` 中 `STAGE00_READY=yes`。

---

## 路径自动发现机制

```
discover_paths.sh 的逻辑：

1. 扫描固定可能的路径
   REPO_ROOT/kernel-src/linux-5.15.10/
   REPO_ROOT/kernel-src/linux/
   REPO_ROOT/kernel-src/busybox-1.36.1/

2. 对每个路径检查是否存在子目录
   KERNEL_BUILD_ARM64/ = build/arm64 或 output/arm64
   KERNEL_IMAGE_ARM64  = build/arm64/arch/arm64/boot/Image
   BUSYBOX_BIN_ARM64  = busybox-1.36.1/output/arm64/bin/busybox

3. 如果自动发现失败，允许 local.env 手动覆盖
```

---

## 工具链检查

```
check_host_tools.sh 检查：

必须项（arm64 路线）：
  - qemu-system-aarch64
  - aarch64-linux-gnu-gcc
  - arm64 BusyBox 静态链接

可选项（x86 优先路线）：
  - qemu-system-x86_64
  - gcc
  - x86 BusyBox
```

---

## STAGE00_READY 判断逻辑

```
判断标准：

STAGE00_READY = yes 需要同时满足：
  1. KERNEL_IMAGE_ARM64 存在（或 TARGET_ARCH=host）
  2. BUSYBOX_BIN_ARM64 存在（或 TARGET_ARCH=host）
  3. QEMU_BIN 存在
```

---

## 与 stage01 的关系

- **stage01**：在验证过的环境上，开始写第一个 net_device 驱动
- **stage06**：把 TARGET_ARCH=arm64 作为独立迁移任务

stage00 的产出是整个 netdev 的"底座"：
- env/stage00.env（架构变量）
- output/discovered_paths.env（自动发现的路径）
- output/host_tools.txt（工具可用性）
- output/stage00_report.md（环境验收结果）
