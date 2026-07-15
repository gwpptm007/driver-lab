# 04. 控制面、数据面与观测面

把所有工作塞进 fast path 会让系统难以调试和演进。网络加速项目至少应分成控制面、数据面与观测面；必要时再独立出恢复/运维面。

## 4.1 三个平面各自做什么

| 平面 | 职责 | 典型对象 | 不应承担的事情 |
| --- | --- | --- | --- |
| 控制面 | 配置拓扑、队列、规则、MR/QP 连接、策略版本 | netlink、devlink、ethtool、tc、DPDK init、RDMA CM/TCP bootstrap | 每包决策和同步日志 |
| 数据面 | 接收、分类、修改、转发、post WR、poll completion | NAPI/XDP、PMD、AF_XDP ring、QP/CQ、eSwitch | 慢速配置查询、阻塞 I/O |
| 观测面 | 计数、采样、trace、health、证据归档 | eBPF、metrics、tc counter、ethtool stats、devlink health | 改变生产流量语义或逐包打印 |

控制面成功只证明尝试配置；数据面计数才证明路径被执行；观测面把二者关联为可验证结论。

## 4.2 版本化与原子切换

规则、队列配置、连接元数据都应具有版本或 generation：

1. 控制面创建新版本，完成校验与资源预留；
2. 数据面以原子指针/epoch 切到新版本；
3. 旧版本等待 in-flight 包、WR 或规则引用归零；
4. 观测面记录切换时间、旧/新版本和失败原因；
5. 失败时回到已知健康版本。

无论是 DPDK 的 flow table、eBPF map、tc rule，还是 RDMA 远端 MR 元数据，这个模式都比就地改写正在被 dataplane 读取的对象安全。

## 4.3 Offload 的三个验证面

以 tc flower 为例：

| 证据 | 说明 |
| --- | --- |
| 控制面 | rule 下发是否成功、是否标记 in_hw、驱动/firmware 是否支持 |
| 数据面 | 专门测试流量是否命中、端口/规则计数是否按预期变化 |
| 观测面 | tc、ethtool stats、devlink health 是否相互一致，是否有 fallback/error |

只有三者闭合，才能说该规则在硬件路径中命中。同理，RDMA QP 建成不等于数据写入正确；DPDK port 启动不等于流量走了预期队列。

## 4.4 控制面不能阻塞数据面

数据面需要预分配、批量和无阻塞快路径。配置刷新、规则编译、日志上传、DNS/认证、健康上报应该在控制线程或独立服务执行。若必须让配置影响每个包，使用只读快照、RCU/epoch 或稳定 map 查表，并把慢路径与错误处理外移。

## 4.5 最小可运维接口

一个可交付的加速服务至少输出：

- 当前版本、端口/队列/QP/规则状态；
- 流量与错误计数、队列/slot 高水位；
- 设备/firmware/NUMA/affinity 运行快照；
- 最近状态切换与有限的错误上下文；
- 降级或回滚状态，以及当前是否真的 offload 的明确标志。

下一篇：[05：性能方法与 NUMA](05_PERFORMANCE_METHOD_AND_NUMA.md)。
