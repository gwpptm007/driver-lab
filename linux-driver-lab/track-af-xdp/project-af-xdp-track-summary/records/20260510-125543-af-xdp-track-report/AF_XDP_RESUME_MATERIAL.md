# AF_XDP Resume Material

## 简历 bullet 候选

### 稳健版本

```text
补充 Linux 原生用户态数据面 AF_XDP 实验链路，完成 XDP attach、XSKMAP redirect、AF_XDP socket/UMEM/ring 初始化、copy/zero-copy 能力探测和 mini forwarder 骨架，形成从内核 XDP 到用户态 packet processing 的可复现实验工程。
```

### 偏工程版本

```text
设计并实现 AF_XDP 学习与验证工程，覆盖 XDP BPF 程序加载、AF_XDP UMEM 注册、FILL/RX/TX/COMPLETION ring 管理、copy/zero-copy 模式探测、drop/reflect mini forwarder，并通过脚本化 records/review bundle 固化验证过程。
```

### 和 DPDK 组合版本

```text
构建 Linux 网络数据面实验体系，覆盖内核 netdev/XDP、DPDK 用户态 PMD fast path 与 AF_XDP 原生用户态收发路径，能够对比 PMD/mbuf/hugepage 与 XDP/XSKMAP/UMEM/rings 的设计差异和适用边界。
```

## 当前不要写得太满的说法

暂时不要写：

```text
AF_XDP zero-copy 线速转发
完成高性能 AF_XDP 网关
生产级 AF_XDP 转发器
```

除非后续补齐真实 traffic、TX reflect 和性能压测 records。
