# Resume Material

## 推荐项目标题

```text
Linux 网络数据面学习与作品集
```

或：

```text
Linux Network Data Plane Portfolio
```

## 简历 Bullet 稳健版

```text
- 构建 Linux 网络数据面学习与作品集项目，覆盖 kernel netdev、真实 Linux NIC 驱动源码、虚拟化网络、DPDK 用户态 fastpath、AF_XDP 原生 fastpath 与 eBPF observability，沉淀 README、scripts、records 和 final reports。
- 完成 netdev stage00~stage14，系统实现并验证 net_device、skb、NAPI、RX/TX ring、多队列、MSI-X、page_pool、ethtool、offload 与 XDP 入口，建立 Linux 网络驱动主路径模型。
- 阅读并映射 virtio_net/e1000e 真实驱动源码，完成 virtio_net runtime observe、ethtool stats mini patch、napi_poll trace chain 和 before/after 证据整理。
- 搭建 tap/bridge/vhost/virtio 虚拟化网络实验，验证 host/guest L2 路径、vhost kick/notify 机制和 two-guest bridge flow。
- 基于 DPDK 21.11 完成 hugepage、PMD、testpmd、vhost-user/virtio-user、自写 l2fwd-lite/fastpath-lite 和 media-gateway-lite，验证 pcap PMD 路径下 UDP traffic/forwarding/rewrite。
- 完成 AF_XDP 四阶段实验，验证 XDP attach、XDP_REDIRECT、XSKMAP、AF_XDP socket、UMEM、FILL/RX/TX/COMPLETION rings 和 mini forwarder frame 生命周期。
- 实现 eBPF 网络路径观测工具，输出 per-interface、per-CPU、RX/GRO/TX/drop reason 和 path invariant 报告，用于网络路径定位和结果解释。
```

## 压缩版

```text
- 构建 Linux 网络数据面作品集，覆盖 kernel netdev、virtio_net/e1000e 真实驱动源码、tap/bridge/vhost 虚拟化网络、DPDK 用户态 fastpath、AF_XDP 原生 fastpath 与 eBPF observability；完成 netdev stage00~stage14、virtio_net patch/trace、DPDK media-gateway-lite UDP traffic/forwarding/rewrite、AF_XDP UMEM/rings mini forwarder 和 eBPF RX/TX/drop 观测报告，沉淀可复现实验脚本、records 与 final reports。
```

## 面试项目描述

```text
这个项目是我对 Linux 网络数据面的系统化收口。从内核 netdev 教学驱动开始，我先建立 skb、NAPI、ring、queue、MSI-X、page_pool、ethtool、offload、XDP 的主路径模型；然后迁移到 virtio_net/e1000e 真实驱动源码阅读和低风险 patch；再扩展到 tap/bridge/vhost 虚拟化网络、DPDK 用户态 fastpath、AF_XDP 原生 fastpath；最后用 eBPF 做 RX/TX/drop 观测。项目里每个阶段都有脚本、records 和报告证据。
```

## 边界说明

面试时建议主动说明：

```text
当前项目是实验型 Linux 网络数据面作品集。DPDK media-gateway-lite 已在 pcap PMD 路径下验证 traffic/forwarding/rewrite，但不是生产级媒体网关；AF_XDP 已验证 UMEM/rings 和 mini forwarder，但没有完成真实 NIC zero-copy 大规模压测。
```
