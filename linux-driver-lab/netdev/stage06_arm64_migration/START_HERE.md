# START_HERE / stage06_arm64_migration

## 优先阅读

**[docs/00_USER_GUIDE.md](docs/00_USER_GUIDE.md)** — 本阶段使用指南：
- stage06 是做什么的
- 在测试机上如何执行
- 每一步验证什么、如何拿结果
- 产物与本地文件对应关系
- 原理图：我们学到了什么
- 常见问题与修复方案

---

## 先读什么

### 第一步
看 `docs/01_STAGE_GOAL_AND_BOUNDARY.md`

明确本阶段不是“新功能驱动阶段”，而是：
- 平台迁移
- 兼容层抽象
- 构建/运行/回归收口

### 第二步
看 `docs/02_MIGRATION_STRATEGY.md`

理解：
- 什么保持 northbound 不变
- 什么是 southbound 需要替换的
- 为什么 stage06 要以 stage04 为主迁移对象

### 第三步
看 `docs/04_BUILD_AND_RUN_FLOW.md`

明确：
- host / qemu-x86_64 / qemu-arm64 三条路径
- 每条路径需要哪些工具
- 什么时候只做 dry-run，什么时候可以真的 build/run

### 第四步
看 `include/netdev_kcompat.h`

这里是本阶段最“硬”的代码交付之一：
- NAPI 兼容包装
- `u64_stats` 兼容包装
- 常用平台/版本判断宏

### 第五步
执行：

```bash
make smoke
```

它会自动完成：
- 环境检查
- 解析 host / qemu-x86_64 / qemu-arm64
- 生成平台矩阵
- 生成迁移差异报告
- 生成 stage06 阶段报告
### 第六步
看新增的 3 份收口文档：

- `docs/STAGE06_CLOSEOUT_EXECUTION_CHECKLIST.md`
- `docs/STAGE06_ACCEPTANCE_CHECKLIST.md`
- `docs/STAGE06_MIGRATION_MAPPING.md`

这三份文档分别回答：
- 现在先改什么
- 怎么判断 stage06 真正完成
- 哪些逻辑不变、哪些是平台差异

### 第七步
再看：

- `docs/STAGE06_KNOWN_ISSUES.md`

它把当前真实踩过的 ARM64 / rootfs / kernel config / 路径问题收成了问题清单。
