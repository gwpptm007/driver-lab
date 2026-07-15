# 08. 可靠性、安全与多租户

性能路径不能以绕过内核为名绕过隔离、错误处理和资源治理。越接近 NIC DMA、RDMA rkey 或 eSwitch rule，错误的影响范围越大，恢复边界越必须清楚。

## 8.1 三种隔离边界

| 边界 | 典型机制 | 要保护什么 |
| --- | --- | --- |
| CPU/进程 | cgroup、CPU pinning、进程权限、IPC | 不让一个 worker 饥饿其他服务或越权控制设备 |
| 内存/DMA | IOMMU、VFIO、UMEM/MR 注册、lkey/rkey | 不让设备或 peer 访问错误缓冲区 |
| 网络/租户 | VLAN/VXLAN、VF/SF、representor、ACL、eSwitch | 不让流量、规则或计数跨租户泄漏 |

这些边界必须共同工作。VF 隔离不能弥补错误 rkey；IOMMU 也不能证明 eSwitch ACL 已命中。

## 8.2 RDMA 的特殊安全模型

RDMA MR 将地址范围、length 和访问权限交给 RNIC/provider 管理。应用仍需约束远端元数据：

- 对协商得到的 remote address、rkey、length 做溢出与边界验证；
- 最小化 MR 大小与访问 flags，避免把长生命周期大块内存暴露给不必要 peer；
- 将 QP、PD、MR、连接身份与租户映射保存到控制面；
- 在连接重建、权限变化或异常时，停止投递并按顺序处理 outstanding WR；
- 不把 local CQE 解释为远端业务授权、持久化或去重确认。

重试必须建立在远端幂等/去重协议上。对状态未知的 WRITE 盲目重发可能造成双写。

## 8.3 多租户下的队列与缓存

共享 queue、mempool、CQ 或 hardware table 时需要配额、限速和可见性边界：

| 资源 | 风险 | 最小治理 |
| --- | --- | --- |
| RX/TX queue | noisy neighbor、head-of-line blocking | RSS/queue 分配、per-tenant rate/occupancy 指标 |
| buffer pool/UMEM | 单租户耗尽、跨租户复用错误 | 配额、水位、owner/generation 检查 |
| RDMA QP/CQ/MR | CQ 风暴、注册内存耗尽、权限混乱 | per-tenant 生命周期、CQ budget、MR 上限 |
| eSwitch flow/counter | 表耗尽、规则覆盖、统计泄漏 | namespace/priority、资源配额、拒绝与审计 |

仅有“性能正常”不足以证明多租户安全；还应验证配额耗尽、规则冲突、连接重置和统计隔离。

## 8.4 故障域与降级

| 故障 | 不安全的反应 | 更安全的反应 |
| --- | --- | --- |
| queue 满 | 覆盖旧 descriptor | 显式 drop/限流，记录压力 |
| CQE error/QP error | 继续 post 或复用未知 buffer | 隔离 QP，处理 outstanding/flush，进入恢复状态机 |
| offload 下发失败 | 悄悄软件回退并报告成功 | 标记降级，记录原因和实际执行位置 |
| NIC/DPU health 告警 | 只重启应用 | 保存状态，按设备恢复流程切换/回退 |
| 控制面失联 | 数据面继续接受新策略 | 冻结变更，使用最后已验证版本或 fail closed |

降级策略必须在压测前定义并演练。故障时吞吐下降可以接受，越权、内存破坏和无证据的静默回退不可以。

## 8.5 安全审计与最小日志

审计记录应包含资源创建/销毁、权限变更、规则版本、offload 结果、QP/MR 绑定、异常 completion 和 health 事件。数据面日志默认只记摘要和标识，避免记录敏感 payload；高频事件使用计数和采样，避免日志本身成为拒绝服务入口。

下一篇：[09：证据等级与对外表述](09_EVIDENCE_LEVELS_AND_CLAIM_DISCIPLINE.md)。
