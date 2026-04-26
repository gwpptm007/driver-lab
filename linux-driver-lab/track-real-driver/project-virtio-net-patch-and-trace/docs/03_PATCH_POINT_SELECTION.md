# 03_PATCH_POINT_SELECTION

## 选点原则

这个项目里的 patch 点要同时满足：

1. 小
2. 稳
3. 能验证
4. 能解释
5. 能和前面的 Lab 串起来

## 当前最推荐的方向

### 首选：ethtool / stats / control-plane 相关小 patch
原因：
- 风险最低
- 和 `stage12` 连续性最强
- before/after 最容易做
- 最适合做第一次真实驱动 patch

### 次选：围绕 queue/poll 的轻量观测增强
原因：
- 和 `queue-poll-observe` 连续性强
- 适合作为“观测型 patch”

## 当前不建议
- 直接改重的 TX/RX 主路径语义
- 一上来改 XDP 动作逻辑
- 过大范围的结构重排

## 选点输出

真正开工前，建议先补：
- `records/<ts>/PATCH_POINT_NOTE.md`
- `records/<ts>/RISK_NOTE.md`
