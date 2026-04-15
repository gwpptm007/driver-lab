# stage06_arm64_migration

## 阶段定位

这是 netdev 主线的收口阶段：在已经完成 `stage01 ~ stage04` 的 netdev 本体学习、并完成 `stage05` 的 `virtio-net` 对照与平台参数化准备之后，正式把主线迁到 **ARM64 / QEMU**，并形成一套可复用的跨平台构建与回归方法。

本阶段的重点不是新增一个全新的驱动功能，而是把前面已经做出来的成果，升级成：

- **可切换平台**（host / qemu-x86_64 / qemu-arm64）
- **可切换工具链**（native gcc / aarch64 cross toolchain）
- **可生成差异报告**（构建、运行、回归、观测）
- **可复用兼容层**（为 stage04 及后续驱动准备）

## 本阶段最重要的判断

- `stage01 ~ stage04` 解决的是 **netdev 本体学习问题**
- `stage05` 解决的是 **virtio-net 对照 + 平台参数化准备问题**
- `stage06` 解决的是 **ARM64 迁移与跨平台收口问题**

所以 stage06 的核心产出不是“再写一份更花的驱动”，而是：

1. 明确迁移什么
2. 明确哪些差异来自平台
3. 明确哪些逻辑保持不变
4. 明确如何在 ARM64 上复现前面的阶段闭环

## 当前落地内容

当前目录已经落下：

- 平台环境解析脚本
- host / qemu-x86_64 / qemu-arm64 参数解析
- stage04 目标平台构建脚本
- ARM64 QEMU dry-run 命令生成
- stage04 ↔ stage06 迁移差异报告骨架
- 可复用内核兼容层头文件（`include/`）

## 建议先看

1. `START_HERE.md`
2. `docs/01_STAGE_GOAL_AND_BOUNDARY.md`
3. `docs/02_MIGRATION_STRATEGY.md`
4. `docs/04_BUILD_AND_RUN_FLOW.md`
5. `docs/05_DIFF_AND_ACCEPTANCE.md`
6. `docs/STAGE06_CLOSEOUT_EXECUTION_CHECKLIST.md`
7. `docs/STAGE06_ACCEPTANCE_CHECKLIST.md`
8. `docs/STAGE06_MIGRATION_MAPPING.md`
9. `docs/STAGE06_KNOWN_ISSUES.md`
10. `include/netdev_kcompat.h`
11. `scripts/smoke.sh`

## 常用命令

```bash
cd linux-driver-lab/netdev/stage06_arm64_migration
make report
make matrix
make resolve-host
make resolve-arm64
make diff
make dryrun-arm64
make smoke
```

## 一句话总结

stage06 的目标不是“从零再学一遍网络驱动”，而是：

> **把前面已经学会、已经测过的 netdev 主线迁到 ARM64，并把整个实验方法做成平台可配置、可比较、可复盘的版本。**


## 本次最新补充

本次在不改动主代码基线的前提下，补进了 stage06 收口所需的关键执行文档：

- `docs/STAGE06_CLOSEOUT_EXECUTION_CHECKLIST.md`
- `docs/STAGE06_ACCEPTANCE_CHECKLIST.md`
- `docs/STAGE06_MIGRATION_MAPPING.md`
- `docs/STAGE06_KNOWN_ISSUES.md`
- `reports/README.md`

这些文档的作用是把当前已经形成的 ARM64 迁移框架，进一步收成“可复现、可验收、可继续承接 stage07”的阶段节点。
