# 01_GOAL_AND_SCOPE

## 目标

这个实验的目标不是“做一个大 patch”，而是做一个：

- 小
- 可验证
- 风险低
- 能体现对真实驱动理解

的真实补丁实验。

## 为什么是 ethtool / stats

### 1. 和已有阶段最连续
和你自己的：
- `stage12_ethtool_control_plane`
映射最强。

### 2. 风险比改主路径低
- 不直接改 RX/TX 数据面语义
- 行为验证相对简单
- 更容易稳定出 first patch

### 3. 很适合作为“真实驱动补丁入门”
你要的不是第一步就搞很重，而是先形成一个：
- patch
- baseline
- before/after
- review note
的完整闭环。

## 范围

### 当前应该做的
- stats/ethtool 控制面相关 patch
- 补丁说明
- baseline 收集
- before/after 对比
- report / review note

### 当前不做的
- 重 TX/RX 语义重构
- 大型 capability 扩展
- 大范围性能优化
- 多驱动并行 patch
