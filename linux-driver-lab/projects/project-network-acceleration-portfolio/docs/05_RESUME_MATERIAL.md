# 05_RESUME_MATERIAL

## 1. 简历项目标题

Linux High-Performance Network Acceleration Lab

## 2. 简历描述版本 A

构建 Linux 高性能网络加速学习与验证项目，覆盖 kernel netdev/NAPI/ring/XDP、virtio_net/e1000e 源码阅读、tap/bridge/vhost 虚拟网络、DPDK 用户态 fastpath、AF_XDP、eBPF 网络可观测性和 RDMA verbs/RoCEv2。通过脚本化测试、日志证据和阶段报告验证关键路径，并明确区分 Soft-RoCE、pcap PMD、veth/虚拟环境与真实硬件性能边界。

## 3. 简历描述版本 B

完成 DPDK/RDMA/AF_XDP/eBPF 网络加速实验体系：实现并验证 DPDK pcap PMD fastpath、AF_XDP socket/UMEM/ring、RDMA RC client/server、RDMA SEND latency 与 batch/inline/selective/CQ polling 调参框架；补充 CPU affinity 与 `numactl node0` 绑定证据，形成可复现测试记录、架构图和面试材料。

## 4. 可展开亮点

- Linux netdev：理解 skb、NAPI、ring、queue、XDP hook。
- DPDK：理解 PMD polling、mbuf/mempool、burst、hugepage、RSS/NUMA/VFIO 边界。
- AF_XDP：验证 XDP redirect、XSKMAP、UMEM 和 RX/TX rings。
- eBPF：建立 RX/TX/drop/cross-path 观测方法。
- RDMA：验证 verbs 对象生命周期、MR/QP/CQ、RC SEND/RECV、WRITE/READ、UD/RoCEv2。
- RDMA perf：实现 SEND latency、batch WR、inline、selective signaling、CQ polling、RTT、CPU affinity、NUMA node0 绑定路径。

## 5. 不能写过头的内容

不要写：

- “完成生产级 RDMA 性能优化”
- “完成 SmartNIC/DPU offload”
- “完成真实 RNIC benchmark”
- “完成 DPDK 生产多队列/RSS/NUMA 全量调优”

更准确的写法：

- “基于 Soft-RoCE 验证 RDMA verbs 模型与调参框架”
- “基于 pcap PMD 验证 DPDK fastpath 逻辑闭环”
- “整理 SmartNIC/DPU offload 后续路线图”
- “明确记录虚拟环境和真实硬件的性能结论边界”
