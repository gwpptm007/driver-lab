# 07_SHARE_AND_INTERVIEW_NOTES

## 这个实验怎么讲

### 1. 为什么现在做 queue/poll observe
因为 `source-dive` 已经给出理论模型，`runtime-observe` 已经有 baseline，  
现在最自然的一步就是把 “事件推进模型” 真正跑出运行期证据。

### 2. 这个实验承接了什么
- `source-dive` 的 Round2 / Round3
- `runtime-observe` 的 idle / ping baseline

### 3. 这个实验的价值
- 它让“queue / poll / callback”不再只是文档里的概念
- 它为后续 patch / tracing 提供最直接的观察基础
