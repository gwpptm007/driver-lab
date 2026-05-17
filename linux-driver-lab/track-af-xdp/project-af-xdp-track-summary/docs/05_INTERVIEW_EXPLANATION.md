# Interview Explanation

## 30 秒说明

我在驱动和 DPDK 之后，继续做了 AF_XDP track。这个方向主要验证 Linux 原生的用户态数据面能力：先用 XDP 程序在驱动收包早期做 PASS/DROP/REDIRECT，再通过 XSKMAP 把流量导入 AF_XDP socket，用户态负责 UMEM、FILL/RX/TX/COMPLETION rings 的管理。它和 DPDK 的区别是 AF_XDP 仍然依赖 Linux 驱动和 XDP 生态，适合做和内核网络路径结合更紧的用户态 fast path。

## 重点术语

```text
XDP: 驱动收包早期的 BPF hook
XSKMAP: XDP redirect 到 AF_XDP socket 的 BPF map
UMEM: AF_XDP 用户态/内核共享 packet frame 内存
FILL/RX/TX/COMPLETION rings: AF_XDP 收发和回收描述符环
copy mode: 兼容性强，性能较弱
zero-copy mode: 性能更好，但依赖驱动支持
```

## 面试中怎么避免夸大

可以说：

```text
目前已经完成 AF_XDP track 的工程化落地：XDP attach 基础验证、AF_XDP socket/rings 实验、copy/zero-copy 探测实验、mini forwarder 项目骨架。部分真实流量和 zero-copy 支持还需要结合测试机环境继续补测。
```

不要说：

```text
已经完成高性能 AF_XDP 转发器并压测达到线速。
```
