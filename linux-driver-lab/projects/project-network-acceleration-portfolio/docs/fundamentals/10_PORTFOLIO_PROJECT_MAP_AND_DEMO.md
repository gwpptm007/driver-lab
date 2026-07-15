# 10. 作品集项目地图与演示

这个作品集的价值不在于列出很多目录，而在于能够沿同一个问题逐层解释：为什么该路径存在、如何验证、何时该换技术、哪里停止下结论。

## 10.1 各 track 的角色

| 模块 | 解决的核心问题 | 作品集中的位置 |
| --- | --- | --- |
| kernel netdev / real driver | NIC queue、NAPI、descriptor、内核驱动边界 | 从硬件接收理解上层快路径 |
| virtual net | tap/veth/bridge/VLAN/vhost 的 L2/L3 边界 | 理解虚拟化与 host 拓扑 |
| DPDK / advanced | PMD、mbuf、mempool、burst、RSS、NUMA | 专核 userspace packet processing |
| AF_XDP | XDP hook、UMEM、rings、copy/zero-copy 边界 | 内核原生 userspace fast path |
| eBPF observability | RX/TX/drop 路径证据 | 不改变主语义地观察路径 |
| RDMA core/performance | MR、QP、CQ、verbs、affinity | remote memory access 与完成模型 |
| DPDK-RDMA Gateway | packet ingress 与 RDMA completion 的组合契约 | 项目级跨技术闭环 |
| SmartNIC/DPU map | representor/eSwitch/offload 的硬件演进 | 从 host path 向硬件下沉的路线 |

## 10.2 十分钟演示结构

### 第 1 分钟：定义问题

说明网络加速不是单一技术竞赛，而是路径、所有权、执行位置和证据的问题。给出当前工作负载假设，例如小包转发、远端数据搬运或虚拟化东西向流量。

### 第 2 至 4 分钟：解释 host 路径

从 netdev/NAPI/queue 说起，解释 skb 路径成本；再说明 XDP/AF_XDP 与 DPDK 分别绕开什么、保留什么。不要只说绕开内核。

### 第 5 至 6 分钟：解释异步内存路径

说明 RDMA 的 MR/QP/CQ 与 packet path 的差异，再展示 DPDK-RDMA Gateway 如何用 staging、SPSC、generation 和 CQE 处理跨路径所有权。

### 第 7 至 8 分钟：展示证据

按 E1/E2 等级展示原始 test record、输入、计数守恒、边界环境与失败项。数字必须带环境。

### 第 9 至 10 分钟：说明硬件演进

解释真实 NIC/RNIC、NUMA、representor、tc in_hw、devlink health 需要补什么证据；说明不把 RXE/pcap 结果夸大为硬件性能。

详细演示顺序见 [../07_PORTFOLIO_DEMO_RUNBOOK.md](../07_PORTFOLIO_DEMO_RUNBOOK.md)。

## 10.3 可复用的项目叙事

~~~text
需求：降低某条数据路径的 CPU 或尾延迟，同时保持可观测和可回退。
基线：记录现有路径与流量/资源/错误。
选择：基于协议、CPU、内存、硬件和运维约束选择最小加速机制。
实现：先定义 buffer、queue、完成与错误所有权。
验证：以守恒计数和对照实验验证软件路径。
演进：在真实硬件上补 NUMA、offload、健康和长稳证据。
~~~

这段叙事可用于设计评审、面试和项目 README，不依赖某个具体框架。

## 10.4 演示前检查

- 能否说清当前结论属于 E0、E1、E2、E3 还是 E4？
- 是否能指出一项具体的原始证据文件？
- 是否能解释一个 buffer 从接收至回收的 owner？
- 是否能说明队列满、CQE error 或 offload 失败时的行为？
- 是否能给出真实硬件下一步的最小验收，而不是泛泛说上 SmartNIC？

下一篇：[11：扩展路线与设计检查表](11_EXTENSION_ROADMAP_AND_DESIGN_CHECKLIST.md)。
