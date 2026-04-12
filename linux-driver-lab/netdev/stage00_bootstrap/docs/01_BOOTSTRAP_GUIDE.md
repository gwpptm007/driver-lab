# stage00_bootstrap 是什么

## 定位

stage00_bootstrap 是 netdev 主线的第零步，**不写驱动代码，只做环境验证**。

## 目标

1. **架构中立**：不默认 ARM64，默认 `TARGET_ARCH=host`
2. **平台可参数化**：工具链和路径通过变量注入，不写死
3. **环境验证**：进入 stage01 之前，确认工具链可用

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
