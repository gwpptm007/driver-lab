# 06_INTERVIEW_EXPLANATION

面试可以这样讲：

> 我在 netdev/XDP 基础后，又做了一条 AF_XDP 用户态数据面路线。先验证 XDP attach 和 XSKMAP redirect，再实现用户态 UMEM、FILL/RX/TX/COMPLETION ring 初始化，最后整理成一个 mini forwarder。这个 forwarder 默认走 skb/copy 兼容路径，支持 drop 和 reflect 两种模式，用来验证用户态收包、frame 回收和 TX completion。zero-copy 能力单独做 probe，因为它依赖网卡驱动，不应该在 VMware/vmxnet3 环境下强行假设一定可用。

重点表达：

- AF_XDP 是 Linux 原生的用户态收发路径；
- XDP 程序负责 redirect，用户态 XSK 负责 rings；
- UMEM frame 所有权必须在 FILL/RX/TX/COMPLETION 之间正确流转；
- copy/zero-copy 是能力边界，不是功能开关；
- 当前项目先做可复现 smoke，再补真实流量和性能测试。
