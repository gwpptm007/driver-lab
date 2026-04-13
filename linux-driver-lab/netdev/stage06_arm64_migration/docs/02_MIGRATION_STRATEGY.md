# 02_MIGRATION_STRATEGY

## 总策略

stage06 采用“northbound 保持、southbound 迁移”的思路：

### 保持不变的部分（northbound）
- `net_device` 生命周期理解
- `ndo_start_xmit` / NAPI / stats 观察口径
- stage04 的测试方法
- records / output / smoke 报告组织方式

### 需要迁移的部分（southbound）
- 构建路径
- 工具链
- QEMU 机型与参数
- kernel image / rootfs image 路径
- 运行环境假设
- 某些内核 API 差异

## 为什么这比“重写驱动”更合理

因为 stage06 的目标不是功能创新，而是把已有驱动成果推广到另一平台。

如果在这个阶段再大量重写驱动本体：
- 会把平台问题和功能问题混在一起
- 很难判断问题来源
- 会削弱前面阶段的复用价值

## 推荐迁移顺序

1. 先把 host / x86_64 / arm64 参数解析做对
2. 再确保至少一条 ARM64 build 路径可执行
3. 再做 QEMU dry-run 命令生成
4. 最后在真实测试机上完成 run / smoke / records 收口
