# STAGE06_ACCEPTANCE_CHECKLIST

## 文档定位

这份文档用于回答一个问题：

> 什么状态下，`stage06_arm64_migration` 才能被认为是“阶段完成并可对外评审”？

这里不讨论新功能扩展，只讨论 **当前 stage06 的工程化收口是否达标**。

---

## 一、环境验收

### 目标
证明 stage06 已经摆脱个人机器路径依赖，能在准备好的新机器上复现。

### 通过项

- [ ] `resolve-host` 能在无个人路径前提下解析完成  
- [ ] `resolve-x86` 能在无个人路径前提下解析完成  
- [ ] `resolve-arm64` 能在无个人路径前提下解析完成  
- [ ] `env/stage06_arm64_migration.env` 足以表达默认配置  
- [ ] 缺失依赖时脚本能明确报错，而不是 silent fallback  
- [ ] `resolved_*.env` 字段完整且语义稳定

### 最低证据
- `output/resolved_host.env`
- `output/resolved_qemu-x86_64.env`
- `output/resolved_qemu-arm64.env`

---

## 二、构建验收

### 目标
证明 stage06 构建入口清晰、产物稳定、错误可定位。

### 通过项

- [ ] host 构建路径可独立执行  
- [ ] arm64 构建路径可独立执行  
- [ ] 模块产物进入固定输出目录  
- [ ] rootfs / initramfs 产物进入固定输出目录  
- [ ] 构建日志进入固定输出目录  
- [ ] 构建失败时能快速定位是环境问题还是源码问题

### 最低证据
- `output/netdev_stage04.ko`
- `output/build_stage04_qemu-arm64.log`
- 对应的 host 构建日志或构建记录

---

## 三、dry-run 验收

### 目标
证明 stage06 可以独立生成可检查的启动命令，不依赖人工拼接 QEMU 参数。

### 通过项

- [ ] x86 dry-run 脚本可生成  
- [ ] arm64 dry-run 脚本可生成  
- [ ] dry-run 内容与 resolved env 一致  
- [ ] dry-run 脚本可作为 smoke / run 的前置输入

### 最低证据
- `output/arm64_qemu_dryrun.sh`
- x86 对应 dry-run 产物

---

## 四、运行时 smoke 验收

### 目标
证明 stage06 在目标平台上已经具备基本运行闭环。

### 通过项

- [ ] 模块可加载  
- [ ] netdev 注册成功  
- [ ] 至少一轮 TX→RX 闭环成功  
- [ ] NAPI poll 被触发  
- [ ] RX replenish 正常发生  
- [ ] dmesg 中无关键报错  
- [ ] smoke 结果可落盘到 `records/`

### 最低证据
- `records/20260413-arm64-smoke/dmesg.txt`
- 对应的 smoke 摘要或阶段报告

---

## 五、平台差异验收

### 目标
证明 stage06 不只是“某个平台能跑”，而是已经把跨平台差异解释清楚。

### 通过项

- [ ] host / qemu-x86_64 / qemu-arm64 三个平台矩阵可生成  
- [ ] stage04 ↔ stage06 差异报告可生成  
- [ ] 一致项和差异项都有清晰解释  
- [ ] 未解释异常单独列出，未混入正常差异

### 最低证据
- `output/platform_matrix.md`
- `output/stage04_stage06_diff.md`
- `output/stage06_report.md`

---

## 六、文档验收

### 目标
证明 stage06 可以被第三方阅读和复用，而不是依赖作者补充说明。

### 通过项

- [ ] `README.md` 说明阶段定位  
- [ ] `START_HERE.md` 给出最小阅读路径  
- [ ] `docs/00_USER_GUIDE.md` 能指导复现  
- [ ] `STAGE06_MIGRATION_MAPPING.md` 讲清迁移映射  
- [ ] `STAGE06_KNOWN_ISSUES.md` 讲清常见问题与修复  
- [ ] 本文档可作为最终验收表使用

---

## 七、阶段通过判定

建议用下面的原则判断：

### 可内部继续推进
- 环境解析和构建大体稳定
- 文档还不完整
- 差异解释还没完全写清

### 可作为正式阶段节点
必须同时满足：
- 环境验收通过
- 构建验收通过
- dry-run 验收通过
- 运行时 smoke 验收通过
- 平台差异验收通过
- 文档验收通过

---

## 八、一句话验收结论模板

当所有关键项完成后，建议在阶段报告中写成：

> `stage06_arm64_migration` 已完成从 stage04 主逻辑到 host / qemu-x86_64 / qemu-arm64 的迁移收口，具备稳定的环境解析、构建、dry-run、smoke、差异分析和文档闭环，可作为后续 stage07 的平台基座。
