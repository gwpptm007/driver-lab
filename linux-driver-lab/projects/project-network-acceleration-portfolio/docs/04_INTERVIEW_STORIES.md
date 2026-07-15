# 04_INTERVIEW_STORIES

## 1. 总故事

可以用这条主线讲：

> 我先从 Linux kernel netdev 建立网络驱动基础模型，理解 skb、NAPI、ring、queue、XDP；再看真实驱动和虚拟化网络，把模型映射到 virtio_net、e1000e、tap/bridge/vhost；然后进入 DPDK 和 AF_XDP 两条用户态 fastpath 路径；最后扩展到 RDMA，把数据路径从 packet processing 推进到 registered memory、QP/CQ 和 one-sided access。每个阶段都保留脚本、日志、测试记录和边界说明。

## 2. DPDK 故事

重点：

- DPDK 的核心是用户态 PMD polling。
- mbuf/mempool/hugepage 是内存和包表示基础。
- burst、多队列、RSS、NUMA 是性能调优关键变量。
- pcap PMD 验证逻辑，不等于真实 NIC 性能。

一句话：

> 我不是只会启动 testpmd，而是用 C app 和 pcap PMD 验证了 classify/rewrite/forwarding 的软件闭环，再把它和真实硬件边界分开讲。

## 3. AF_XDP 故事

重点：

- XDP hook 是内核入口。
- XSKMAP redirect 把包导向 AF_XDP socket。
- UMEM 和四个 ring 是用户态收发核心。
- copy/native/zero-copy 支持依赖驱动和网卡。

一句话：

> AF_XDP 在 DPDK 和内核网络之间提供一个折中：保留 Linux 原生入口，又把数据面拉到用户态。

## 4. RDMA 故事

重点：

- RDMA 不是 socket 快一点，而是 verbs 对象和 registered memory。
- MR 暴露地址和 rkey，QP 负责传输语义，CQE 负责完成确认。
- RC SEND/RECV、WRITE、READ 的 CPU 参与方式不同。
- performance tuning 需要结合 batch WR、inline、selective signaling、CQ polling、CPU/NUMA。

一句话：

> 我用 Soft-RoCE 把 RDMA 对象模型和工程化 client/server 跑通，再做了 latency/batch/inline/selective/polling 的调参框架；同时明确 RXE 不能代表真实 RNIC 性能。

## 5. eBPF 故事

重点：

- eBPF 用于验证路径是否真的经过内核、哪里 drop、哪里收发。
- 它不是替代 datapath，而是横切观测工具。
- 对 DPDK/AF_XDP/RDMA 这种绕开部分内核路径的技术，观测边界尤其重要。

一句话：

> 我把 eBPF 当成验证工具，而不是只写 demo：它帮助确认哪些路径仍在内核里，哪些已经绕开内核。

## 6. SmartNIC/DPU 故事

当前只能讲地图，不能讲完成：

- representor
- switchdev
- devlink
- tc flower offload
- OVS offload
- host / embedded control plane 分工

一句话：

> SmartNIC/DPU 是后续路线，我当前已经把 netdev、DPDK、AF_XDP、RDMA 的主干模型打通，下一步会把 tc flower、representor、switchdev、devlink 和 OVS offload 串起来。
