# 02_PATCH_POINT_SELECTION

## 选点原则

当前选点要满足四个条件：

1. **小**
   - 改动范围小
   - 容易解释

2. **稳**
   - 不直接触碰最重的主路径语义
   - 失败风险低

3. **能验证**
   - 有 before/after
   - 有 ethtool / stats / workload 证据

4. **能体现理解**
   - 能说清为什么改这里
   - 能和 `source-dive` / `runtime-observe` 串起来

## 最推荐的选点

### 方向 A：stats 展示增强
适合做：
- 补一项你已经在 runtime observe 中反复使用的统计解释
- 强化某项现有 stats 的对照说明
- 增加一个轻量的统计输出点（前提是改动很小）

### 方向 B：ethtool 可见性增强
适合做：
- 补充一个对外可见的状态说明
- 梳理一个 capability / stats 的展示逻辑

### 方向 C：轻量记录型 patch
适合做：
- 不改主语义，只增强可观测性
- 便于后续 tracing 与评审解释

## 当前不建议的选点

- 直接改 TX queue 主逻辑
- 直接改 RX refill/recycle 主逻辑
- 一开始就碰 XDP 动作语义
- 一开始就跨多个文件做重构

## 选点输出建议

真正开工前，先在 `records/<ts>/PATCH_POINT_NOTE.md` 写清：

- 准备改哪里
- 为什么改这里
- 预期 before/after 是什么
- 为什么这个实验当前风险可控
