# 07. SmartNIC、DPU、representor 与 offload

SmartNIC/DPU 项目不是把 host 软件原封不动地搬到网卡，而是重新划分执行位置和故障域。host、DPU 核、NIC eSwitch 与物理端口共同组成一个系统，控制面、数据面和观测面都必须重新对应。

## 7.1 先区分四个对象

| 对象 | 角色 |
| --- | --- |
| PF/VF/SF | PCIe function 和资源/租户分配边界 |
| representor | host 侧可管理 netdev，代表 eSwitch 中某个端口/function |
| eSwitch | NIC 内部的转发/匹配实体，可执行部分硬件规则 |
| DPU | 具备独立 CPU/内存/OS 的基础设施执行位置，可运行控制或服务逻辑 |

representor 是控制与可观测性接口，不等于业务数据必然经过 host。eSwitch 规则可能在硬件中直接把 VF/SF/物理口流量转发或丢弃。

## 7.2 何时把逻辑下沉

下沉候选通常包括：虚拟交换、ACL、五元组分类、计数、封装/解封装、镜像、QoS、服务链转向和租户隔离。是否值得下沉取决于 host CPU、尾延迟、隔离、可靠性和可运维成本，而不只取决于网卡是否支持某个 feature。

不应下沉的典型情形：规则高频变化而硬件表更新慢；业务需要复杂状态或完整 payload；无法取得命中/错误证据；没有可恢复的回退路径；硬件资源在多租户场景不可预测。

## 7.3 从 host 到 offload 的安全步骤

| 阶段 | 做什么 | 最小验收 |
| --- | --- | --- |
| H0 盘点 | PCI、driver、firmware、NUMA、端口、资源与 health | 可重放的环境快照 |
| H1 拓扑 | 识别 PF/VF/SF、representor 与物理口关系 | netdev 与 devlink port 映射一致 |
| H2 单规则 | 添加一条隔离测试流量的 drop/redirect rule | in_hw 与计数增长 |
| H3 对照 | 同一流量比较 host path 与 eSwitch path | 两端计数和 CPU 变化可解释 |
| H4 故障 | 制造可恢复的规则/端口异常 | health、告警、回滚记录完整 |
| H5 扩展 | 多规则、租户、容量、升级 | 资源水位与降级策略已定义 |

前一阶段没有证据时，不应跳到多租户或性能宣传。

## 7.4 资源与回退

硬件 flow table、counter、encap、hairpin queue、VF/SF 数量都有限。控制面必须能够查询资源、处理拒绝、避免部分下发，并保留软件路径或明确拒绝新策略。不能在硬件表耗尽后悄悄回退到 host 再报告 offload 成功。

切换 switchdev mode、创建/删除 VF、firmware 更新都可能影响业务连接。实验需要独立环境、恢复步骤、版本快照与变更窗口，不把不可逆操作混入性能测试。

## 7.5 与现有作品集的关系

现有 track 提供进入硬件阶段所需的基础：netdev/XDP 解释 host 内核路径；DPDK/AF_XDP 解释用户态队列和内存；eBPF 用于观测；RDMA 解释 RNIC 数据搬运和完成；virtual net 帮助理解 VF/representor 周边的 L2 语义。

当前项目仅提供地图和验收规则，不声称已经在 SmartNIC/DPU 上完成 offload。详细实施入口见 [../06_SMARTNIC_DPU_MAP.md](../06_SMARTNIC_DPU_MAP.md)。

下一篇：[08：可靠性、安全与多租户](08_RELIABILITY_SECURITY_AND_MULTITENANCY.md)。
