# stage03_napi_poll / PLAN

## 目标
NAPI / poll / 中断抑制

## 重点
本阶段重点不是 API，而是为什么需要 NAPI、如何与 ring 配合。

## 任务
- 引入 napi_struct 与 poll 逻辑
- 观察 budget 行为
- 做纯中断与 NAPI 的对比
- 建立 irq/poll/budget 统计项

## 验收口径
见 `../../docs/06_ACCEPTANCE_AND_MILESTONES.md` 中对应里程碑。
