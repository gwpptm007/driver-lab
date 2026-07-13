# DPDK 项目知识地图

## 1. 先选知识缺口，再选项目

不要按目录名称随机点进去。下面的路线把每个项目放回完整数据路径：

```mermaid
flowchart LR
    ENV[环境/绑定/hugepage] --> PMD[testpmd + PMD]
    PMD --> VIRT[vhost-user/virtio-user]
    PMD --> L2[L2 forwarding C app]
    L2 --> FP[parser/classify/rewrite]
    FP --> TRAFFIC[traffic evidence]
    TRAFFIC --> MEDIA[media gateway modules]
    MEDIA --> REVIEW[legacy mapping/summary]
```

## 2. 推荐学习顺序

| 顺序 | 项目 | 进入前应懂 | 项目主要回答 | 完成后应能说清 |
|---:|---|---|---|---|
| 1 | `lab-vmxnet3-testpmd` | NIC/PCI、hugepage、PMD | DPDK 能否发现并驱动端口 | 绑定、EAL、port stats、环境边界 |
| 2 | `lab-vhost-user-basic` | Unix socket、frontend/backend | vhost-user 控制通道如何出现 | socket 角色不等于数据已转发 |
| 3 | `lab-virtio-user-vhost` | virtqueue、共享内存 | 用户态 virtio frontend 如何对接 backend | feature negotiation 与 queue 建立 |
| 4 | `lab-dpdk-l2-forwarding` | mbuf/mempool、RX/TX queue | 最小 C fast path 如何写 | port lifecycle、burst、unsent free |
| 5 | `project-user-space-fastpath` | Ethernet/IP/UDP、ownership | 如何分类、丢弃、改写 | parser 边界、checksum、统计守恒 |
| 6 | `project-fastpath-traffic-test` | 流量拓扑、抓包 | 如何把 smoke 升级为流量证据 | 输入、动作、输出三方证据 |
| 7 | `project-dpdk-media-gateway-lite` | 模块边界、rule/action | 如何组织更接近业务的数据面 | config/port/packet/rule/stats 分工 |
| 8 | `project-dpdk-v17-legacy-review` | 前述完整路径 | 老 DPDK 经验如何映射现代接口 | KNI/UIO/VFIO/vhost 版本边界 |
| 9 | `project-dpdk-track-summary` | 所有实验记录 | 如何形成作品与面试证据 | 能力、证据、限制、下一步 |

## 3. 只想快速补基础

```text
00 mental model
-> 01 kernel/hardware position
-> 02 mbuf/mempool/hugepage
-> lab-vmxnet3-testpmd
-> lab-dpdk-l2-forwarding
```

完成这条最短路径后，至少不会在项目中迷失于 EAL 参数、port、queue 和 mbuf ownership。

## 4. 只想理解虚拟化数据面

```mermaid
flowchart LR
    QEMU[QEMU/VM or virtio-user] --> VIRTIO[virtio frontend]
    VIRTIO --> VQ[virtqueue/shared memory]
    VQ --> VHOST[vhost-user backend]
    VHOST --> DPDK[DPDK application]
```

阅读顺序：

1. `01_KERNEL_AND_HARDWARE_POSITION.md` 的设备托管边界。
2. `03_END_TO_END_DATA_PATH.md` 的 vhost-user 章节。
3. `lab-vhost-user-basic`。
4. `lab-virtio-user-vhost`。

验收时要区分三层：socket 创建、queue/feature 协商、真实 packet counter。前两层通过不能代替第三层。

## 5. 只想写 C fast path

```text
rte_eal_init
-> mempool create
-> port/queue setup
-> rx_burst
-> safe parser
-> classify/action
-> tx_burst
-> free unsent/drop
-> stats and cleanup
```

先读 `lab-dpdk-l2-forwarding/app/main.c`，再读 `project-user-space-fastpath/app/main.c`。前者看骨架，后者看业务分支；不要反过来从复杂 parser 猜初始化流程。

## 6. 知识到代码的索引

| 知识点 | 最小代码入口 | 深入入口 |
|---|---|---|
| EAL 参数 | l2fwd `main()` | media gateway `gateway_config.c` |
| mempool/mbuf | l2fwd `main()` | fastpath parser/forward loop |
| port lifecycle | l2fwd port setup | media gateway `gateway_port.c` |
| Ethernet/IP/UDP | fastpath `main.c` | media gateway `gateway_packet.c` |
| rule/action | fastpath classify | media gateway `gateway_rule.c` |
| statistics | l2fwd loop | media gateway `gateway_stats.c` |
| vhost/virtio | vhost/virtio labs | advanced virtual pipeline |
| 多 queue/RSS | 基础文档概念 | `track-dpdk-advanced/lab-dpdk-rss-multiqueue` |
| pipeline/ring | 并发文档概念 | `track-dpdk-advanced/project-dpdk-flow-pipeline` |

## 7. 每个项目的固定学习动作

进入任一 lab/project，按同一个模板执行：

1. **定位**：在总数据路径中圈出它负责哪一段。
2. **对象**：列出它创建和销毁的 DPDK 对象。
3. **所有权**：画出 mbuf 在每个分支由谁负责。
4. **守恒**：写出 RX、TX、drop、error 的计数关系。
5. **证据**：区分 compile、smoke、traffic、performance。
6. **边界**：说明 vdev/VM/UIO 结果不能证明什么。

## 8. 验收等级不要混淆

| 等级 | 能证明 | 不能证明 |
|---|---|---|
| compile | API/依赖基本匹配 | 程序能运行 |
| smoke | 初始化和主循环可启动 | 有真实流量 |
| traffic | packet/action counters 守恒 | 性能达标 |
| performance | 指定环境中的吞吐/延迟 | 其他硬件同样表现 |

track 中历史状态包括 `PASS_SMOKE`、`PASS_WITH_WARN` 等，阅读时要保留这些边界，不把“项目目录存在”当作“所有路径已实测”。

## 9. 进入 Advanced 的条件

至少能独立解释以下内容再进入 `track-dpdk-advanced`：

- 一个包从 RX descriptor 到 mbuf，再到 TX descriptor 的所有权。
- hugepage、mempool、mbuf 各自解决的问题。
- queue/lcore/NUMA 为什么应一起映射。
- partial TX、ring full 和 RX no-mbuf 分别表示哪层 backpressure。
- smoke、traffic 和 performance 证据的差别。

## 10. 自测

1. 如果忘了 mbuf ownership，应该先进入哪个文档和哪个最小项目？
2. vhost-user socket 创建、virtqueue ready、真实 packet counter 分别属于哪层证据？
3. 为什么推荐先读 l2fwd 骨架，再读 fastpath parser？
4. 当前哪些项目只有 smoke 证据，不能表述为真实转发或性能完成？
5. 进入 Advanced 前应能解释哪三类 backpressure 信号？
