# DPDK Advanced Fundamentals

该目录复用基础 `track-dpdk/docs/fundamentals` 的学习模板，但只讲进阶增量，不重复 EAL、普通 mbuf、基础 port lifecycle。

| 文件 | 职责 |
|---|---|
| `00_ADVANCED_MENTAL_MODEL.md` | 前置门槛、能力地图和证据边界 |
| `01_HARDWARE_QUEUE_STEERING.md` | RSS/RETA/queue/rte_flow |
| `02_ADVANCED_MEMORY_AND_DATA_STRUCTURES.md` | clone/extbuf/ring/hash/cache |
| `03_MULTICORE_PIPELINE_DATA_PATH.md` | 多 worker、ownership、backpressure |
| `04_CONCURRENCY_RCU_QSBR.md` | 动态规则、grace period、回收 |
| `05_PROJECT_KNOWLEDGE_MAP.md` | 知识到 Phase 1-7 映射 |
| `06_PROFILING_AND_DEBUGGING.md` | TSC/perf/xstats/分层排障 |
| `07_RECALL_CARDS.md` | 复习卡和面试式自测 |

每篇保持：总图、精确机制、类比边界、当前代码映射、自测。真实硬件未覆盖的内容用 capability/boundary 表述，不用软件模型替代硬件证据。
