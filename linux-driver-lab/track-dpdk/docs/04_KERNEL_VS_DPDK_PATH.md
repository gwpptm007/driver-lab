# 04_KERNEL_VS_DPDK_PATH

## Kernel path

```text
NIC/vNIC
  -> kernel driver
  -> netdev
  -> NAPI
  -> skb
  -> TCP/IP stack
```

## DPDK path

```text
NIC/vNIC
  -> vfio/uio
  -> PMD
  -> mbuf
  -> user-space app
```

## 核心差异

- DPDK 绕过内核协议栈
- 使用 hugepage 和 mempool 管理包 buffer
- 用 poll/burst 模型减少 syscall/interrupt 开销
