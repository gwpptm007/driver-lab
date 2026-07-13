# DPDK Fundamentals 文档约定

该目录是项目之前的知识前置层。它与 lab/project 文档分工如下：

- fundamentals：回答“为什么、对象是什么、处于完整路径哪里”。
- lab/project docs：回答“当前代码怎么实现、怎么测试、结果是什么”。
- records/reports：保存可复核证据和阶段结论。

## 固定模板

后续 DPDK Advanced 和 RDMA 基础层可复用以下结构，但内容必须按技术域重写：

| 文件 | 固定职责 |
|---|---|
| `00_10_MINUTE_MENTAL_MODEL.md` | 一张总图、术语地图、最短理解路径 |
| `01_KERNEL_AND_HARDWARE_POSITION.md` | 内核、驱动、硬件和用户态边界 |
| `02_CORE_OBJECTS_AND_MEMORY.md` | 核心对象、内存布局、对象关系 UML |
| `03_END_TO_END_DATA_PATH.md` | 初始化到数据完成的完整时序 |
| `04_CONCURRENCY_AND_LIFECYCLE.md` | 并发、状态机、所有权和退出 |
| `05_PROJECT_KNOWLEDGE_MAP.md` | 知识点到现有项目/代码映射 |
| `06_DEBUGGING_PLAYBOOK.md` | 分层排障、命令和症状对照 |
| `07_RECALL_CARDS.md` | 记忆卡、自测题和常见错误表述 |

专家扩展专题：

| 文件 | 固定职责 |
|---|---|
| `08_PACKET_FORMAT_AND_OFFLOADS.md` | 字节序、VLAN/分片/multi-segment、checksum offload |
| `09_ETHDEV_CAPABILITY_AND_PORT_CONFIG.md` | capability、descriptor、queue/offload 协商 |
| `10_PERFORMANCE_AND_OBSERVABILITY.md` | Mpps/Gbps/latency、统计守恒和证据边界 |
| `11_VIRTIO_VHOST_DATA_PATH.md` | virtqueue、vhost-user control 与共享内存数据面 |
| `12_SAFE_ENVIRONMENT_PREPARATION.md` | 管理口保护、bind/restore、hugepage 与凭据安全 |

## 每篇内容要求

1. 先说明解决什么问题。
2. 至少一张总图或布局图。
3. 精确机制与通俗类比同时出现。
4. 明确类比和实验环境的边界。
5. 映射到仓库真实代码/API。
6. 末尾提供自测或可复述结论。

Mermaid 图应优先表达关系、时序和状态；ASCII 图用于 byte layout、内存布局和命令行路径。不要为了图的数量重复正文。
