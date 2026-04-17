# START_HERE — stage06_arm64_migration

## 先读什么

1. `docs/01_STAGE_OVERVIEW.md` — 目标、迁移策略、平台路径
2. `docs/02_USER_GUIDE.md` — 使用方式、build/run、常见问题
3. `docs/03_ACCEPTANCE.md` — 验收标准和检查单
4. `docs/04_DEEP_LEARNING.md` — 深度分析
5. `include/netdev_kcompat.h` — 兼容层代码

## 核心理解

stage06 不是"新功能驱动阶段"，而是"平台迁移与跨平台收口阶段"。

- `stage01 ~ stage04` 解决 **netdev 本体学习问题**
- `stage05` 解决 **virtio-net 对照 + 平台参数化准备问题**
- `stage06` 解决 **ARM64 迁移与跨平台收口问题**

## 当前落地内容

- 平台环境解析脚本（resolve_platform_env.sh）
- host / qemu-x86_64 / qemu-arm64 参数解析
- stage04 目标平台构建脚本
- ARM64 QEMU dry-run 命令生成
- 可复用内核兼容层头文件（`include/`）

## 快速测试

```bash
make smoke
```

## 常用命令

```bash
make resolve-host
make resolve-arm64
make build-stage04-arm64
make dryrun-arm64
make matrix
make diff
make report
```
