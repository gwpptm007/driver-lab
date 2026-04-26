# 06_ACCEPTANCE_AND_REVIEW

## 最低通过标准

1. idle / ping / iperf3 三轮中至少完成两轮
2. 能形成 before/after delta
3. 有一份 trace/log 证据
4. 能说明 queue/poll 事件链在运行期的存在
5. 有一份总结文档

## 标准通过

在最低通过基础上，再满足：
- 三轮 workload 都完成
- 有一份对照表
- 能和 `source-dive` / `runtime-observe` 建立明确映射

## 优秀通过

- 能指出当前观测还不够的地方
- 能自然引出下一步更细 tracing 或小 patch 方向
- 能形成面向评审的简洁报告
