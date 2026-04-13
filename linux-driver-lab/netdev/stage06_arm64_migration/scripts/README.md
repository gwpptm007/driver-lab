# scripts / README

## 核心脚本

- `check_platform_env.sh`
  - 采集 host 侧工具与路径可见性
- `resolve_platform_env.sh`
  - 按 `TARGET_PROFILE` 生成 `resolved_*.env`
- `generate_platform_matrix.sh`
  - 生成 host / x86 / arm64 平台矩阵
- `collect_stage04_stage06_diff.sh`
  - 生成迁移差异报告
- `build_stage04_for_target.sh`
  - 尝试用指定 profile 构建 stage04
- `dryrun_arm64_qemu.sh`
  - 生成 ARM64 QEMU 命令，不实际执行
- `generate_stage06_report.sh`
  - 生成阶段总报告
- `smoke.sh`
  - 串起上述 dry-run 流程
