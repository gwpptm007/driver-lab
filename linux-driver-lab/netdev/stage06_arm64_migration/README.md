# stage06_arm64_migration

> stage06 的目标不是"从零再学一遍网络驱动"，而是把前面已经学会、已经测过的 netdev 主线迁到 ARM64，并把整个实验方法做成平台可配置、可比较、可复盘的版本。

## 本阶段要解决什么

- **可切换平台**：host / qemu-x86_64 / qemu-arm64
- **可切换工具链**：native gcc / aarch64 cross toolchain
- **可生成差异报告**：构建、运行、回归、观测
- **可复用兼容层**：为 stage04 及后续驱动准备

## 核心文档

- [START_HERE.md](START_HERE.md) — 阅读顺序和快速开始
- [docs/01_STAGE_OVERVIEW.md](docs/01_STAGE_OVERVIEW.md) — 目标与迁移策略
- [docs/02_USER_GUIDE.md](docs/02_USER_GUIDE.md) — 使用指南
- [docs/03_ACCEPTANCE.md](docs/03_ACCEPTANCE.md) — 验收标准
- [docs/04_DEEP_LEARNING.md](docs/04_DEEP_LEARNING.md) — 深度分析

## 推荐先看

```bash
cat START_HERE.md
```
