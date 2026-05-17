# XDP to AF_XDP Architecture

## 数据路径

```text
NIC driver RX path
    ↓
XDP program
    ↓
BPF map: XSKMAP
    ↓
bpf_redirect_map()
    ↓
AF_XDP socket
    ↓
UMEM frames
    ↓
RX ring / TX ring
    ↓
user-space poll loop
```

## 核心对象

| 对象 | 作用 |
|---|---|
| XDP program | 在驱动收包早期决定 PASS/DROP/REDIRECT |
| XSKMAP | 保存 queue id 到 AF_XDP socket 的映射 |
| UMEM | 用户态和内核共享的 packet frame 内存区域 |
| FILL ring | 用户态把空闲 frame 交给内核接收包 |
| RX ring | 内核把已收包 frame 描述符交给用户态 |
| TX ring | 用户态把待发送 frame 描述符交给内核 |
| COMPLETION ring | 内核通知用户态 TX frame 已经完成，可回收 |

## copy 与 zero-copy

```text
copy mode:
  驱动/内核会发生拷贝或通用路径处理，兼容性更强。

zero-copy mode:
  NIC/驱动直接使用 UMEM frame，性能更好，但强依赖驱动支持。
```

在 VMware `vmxnet3` 环境里，zero-copy 不一定支持；如果探测失败，应记录为 `NOT_SUPPORTED_ON_THIS_ENV`，而不是测试失败。
