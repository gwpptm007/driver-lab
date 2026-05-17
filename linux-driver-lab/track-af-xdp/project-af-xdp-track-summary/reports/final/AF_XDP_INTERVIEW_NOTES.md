# AF_XDP Interview Notes

## AF_XDP 是什么

AF_XDP 是 Linux 提供的一种高性能用户态 packet socket。它通常和 XDP 配合使用：XDP 程序通过 `bpf_redirect_map()` 把包 redirect 到 XSKMAP 中注册的 AF_XDP socket，用户态通过 UMEM 和 ring 描述符收发包。

## AF_XDP 和 DPDK 的区别

| 维度 | DPDK | AF_XDP |
|---|---|---|
| 驱动模型 | PMD 接管设备 | 复用 Linux 驱动/XDP |
| 内存模型 | hugepage / mbuf | UMEM / frame |
| 收发模型 | RX/TX burst | RX/TX rings |
| 生态关系 | 更偏独立用户态数据面 | 更贴近 Linux 内核网络生态 |
| zero-copy | 依赖 PMD/网卡 | 依赖 AF_XDP 驱动支持 |

## UMEM 和 rings 怎么讲

```text
UMEM 是 AF_XDP 用来承载 packet frame 的共享内存区域。
FILL ring 把空闲 frame 交给内核接收。
RX ring 让用户态拿到已收包 frame。
TX ring 让用户态提交待发送 frame。
COMPLETION ring 通知用户态哪些 TX frame 可以回收。
```

## copy / zero-copy 怎么讲

copy mode 兼容性更强，不要求驱动支持 zero-copy；zero-copy 性能更好，但必须驱动实现对应 AF_XDP zero-copy 能力。测试环境如果是 VMware `vmxnet3`，zero-copy 探测失败是合理结果，应记录为环境不支持，而不是功能失败。

## 当前项目怎么讲

```text
我做了 AF_XDP 的阶段化实验：先验证 XDP attach 和 action 模型，再实现 AF_XDP socket/rings，随后做 copy/zero-copy 探测，最后组合成 mini forwarder。这个项目让我把 XDP、XSKMAP、UMEM、rings 和用户态 poll loop 串起来理解。
```
