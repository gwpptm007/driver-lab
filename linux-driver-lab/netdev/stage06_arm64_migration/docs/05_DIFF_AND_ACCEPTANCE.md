# 05_DIFF_AND_ACCEPTANCE

## stage04 -> stage06 迁移时要看哪些差异

### 构建差异
- `HOST_CC` vs `CROSS_COMPILE`
- `KDIR` / `KERNEL_BUILD_DIR`
- 模块能否生成

### 运行差异
- QEMU 可执行程序
- kernel image / rootfs image 路径
- interface / debugfs / unload 行为

### 观测差异
- `debugfs` 是否一致
- smoke 输出是否一致
- dmesg 关键日志是否一致

## 最低通过口径

### 可接受通过
- 平台矩阵生成正确
- ARM64 参数解析正确
- 至少一条 ARM64 build 路径成立

### 更理想通过
- ARM64 QEMU run 成功
- stage04 smoke 在 ARM64 跑通
- 输出真正的 records 归档

## 推荐沉淀物

- `output/platform_matrix.md`
- `output/stage04_stage06_diff.md`
- `output/stage06_report.md`
- `records/arm64-smoke-*.md`
