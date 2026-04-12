# stage04_ring_dma RX ethertype 问题调试记录

## 测试环境
- 测试机器: 192.168.65.135 (wq7@Ubuntu 22.04)
- 内核版本: 6.8.0-107-generic
- 测试日期: 2026-04-12
- 代码路径: /home/wq7/workspace/driver-lab/linux-driver-lab/netdev/stage04_ring_dma/

## 问题描述

### 原始现象
- TX 路径正常：`tx_packets` 递增，`TX RXIDX=` 打印显示 `CPYLEN` 值正确
- RX 路径表面正常：`RC=0`（netif_receive_skb 成功），但 `rx_packets=0`，`rx_dropped` 递增
- POLL debug 输出显示 `PROTO=86dd`（IPv6）、`0800`（IPv4）或 `0004`，而非期望的 `0x88B7`

### 预期行为
发送 ethertype=0x88B7 的帧时，POLL 中应看到 `PROTO=88B7`

## 根因分析

### 问题1: send_stage04_frame.c 的 ethertype 解析 BUG

**文件**: `tools/send_stage04_frame.c`

**症状**: `./send_stage04_frame nds4 xyz123 88B7 10` 输出 `ethertype=0x0058` 而非 `0x88B7`

**原因**: `strtoul("88B7", NULL, 0)` 以 **decimal** 解析字符串，`'B'` 不是合法十进制字符，所以只解析了 "88" → 0x58

**修复**:
```c
// 修复前
ethertype = (unsigned int)strtoul(argv[3], NULL, 0);

// 修复后（强制 hex 解析）
ethertype = (unsigned int)strtoul(argv[3], NULL, 16);
```

### 问题2: AF_PACKET SOCK_RAW 绑定特定 ethertype 时 frame 格式错误

**文件**: `tools/send_stage04_frame.c`

**原因**: `socket(AF_PACKET, SOCK_RAW, htons(ethertype))` 绑定特定 ethertype 时：
- **发送**: kernel 会在 sendto 数据前**自动添加 Ethernet header**（dst MAC、src MAC、ethertype）
- **接收**: 只接收匹配 ethertype 的帧
- 因此传入 `sendto` 的数据应该是**纯 payload**，不应该包含 Ethernet header

但代码中 `frame[0-5]=dst MAC, [6-11]=src MAC, [12-13]=ethertype, [14...]=payload` 把整个 frame 传给了 sendto，导致 kernel 再加一层 header，变成双层 Ethernet header。

**修复**:
```c
// 修复前（错误）
fd = socket(AF_PACKET, SOCK_RAW, htons((unsigned short)ethertype));

// 修复后
fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));  // 接收所有帧
// 然后手动构造完整 Ethernet frame:
// frame[0-5]=dst MAC, [6-11]=src MAC, [12-13]=ethertype, [14...]=payload
```

### 问题3: 模块卸载时 rmmod 卡死 (used=-1)

**文件**: `driver/netdev_stage04.c`

**症状**: `rmmod netdev_stage04` 始终卡住，`lsmod` 显示 `Used` = -1

**原因**: `stage04_exit()` 清理顺序错误
```c
// 错误顺序
dev_remove_pack(&priv->rx_pkt_type);      // 太早移除协议处理
unregister_netdev(stage04_dev);            // 等待引用释放（可能死锁）
napi_disable(&priv->napi);                 // 太晚停止 NAPI
```

**修复**:
```c
static void __exit stage04_exit(void)
{
	struct stage04_priv *priv;

	if (!stage04_dev)
		return;
	priv = netdev_priv(stage04_dev);

	/* 1. stop NAPI polling first */
	napi_disable(&priv->napi);
	/* 2. stop TX queue */
	netif_tx_disable(stage04_dev);
	/* 3. unregister netdev (this waits for all refs to release) */
	unregister_netdev(stage04_dev);
	/* 4. remove packet type handler */
	dev_remove_pack(&priv->rx_pkt_type);
	/* 5. clean up debugfs */
	stage04_debugfs_exit(priv);
	/* 6. remove NAPI from system */
	netif_napi_del(&priv->napi);
	/* 7. free ring buffers and netdev */
	stage04_cleanup_rings(priv);
	free_netdev(stage04_dev);
	stage04_dev = NULL;
}
```

## 调试过程关键输出

### 错误输出（调试中）
```
TX RXIDX=12 SKBLEN=213 CPYLEN=213 ETH=7879        ← ETH=7879='xy'（payload 开头）
TX RXIDX=12 SKBLEN=213 CPYLEN=213 D0_5=ff:ff:ff:ff:ff:ff D12_15=88:b7:78:79  ← D12=88 B7 正确
POLL IDX=12 LEN=213 PROTO=86dd RC=0              ← PROTO=86dd（IPv6）错误
```

### 正确输出（修复后）
```
[send_stage04_frame] sent 3 frame(s) on nds4 ethertype=0x88b7 payload="xyz123"
[stage04] TX RXIDX=21 SKBLEN=20 CPYLEN=20 D0_5=ff:ff:ff:ff:ff:ff D6_11=24:24:24:24:24:24 D12_15=88:b7:78:79
[stage04] POLL IDX=21 LEN=20 PROTO=88b7 RC=0
```

### TX D12_15 字段含义
- D12_15=88:b7:78:79 → bytes 12-15 of skb->data
  - `88:b7` = ethertype 0x88B7（正确！）
  - `78:79` = 'xy'（payload 前两字节）

## 修复后的调试 print

驱动 TX 路径最终 debug:
```c
pr_info("[stage04] TX RXIDX=%u SKBLEN=%u CPYLEN=%u D0_5=%02x:%02x:%02x:%02x:%02x:%02x D6_11=%02x:%02x:%02x:%02x:%02x:%02x D12_15=%02x:%02x:%02x:%02x\n",
	rx_idx, skb->len, copy_len,
	skb->data[0], skb->data[1], skb->data[2],
	skb->data[3], skb->data[4], skb->data[5],
	skb->data[6], skb->data[7], skb->data[8],
	skb->data[9], skb->data[10], skb->data[11],
	skb->data[12], skb->data[13],
	skb->data[14], skb->data[15]);
```

## 最终结论

**SMOKE TEST: PASS ✅**

| 测试项 | 状态 |
|--------|------|
| 模块加载 insmod | ✅ |
| 接口 UP (ip link set nds4 up) | ✅ |
| 发送 ethertype=0x88B7 帧 | ✅ `ethertype=0x88b7` |
| TX 路径 (CPYLEN 正确) | ✅ |
| TX ETH 字段正确 | ✅ D12_15=88:b7 |
| POLL PROTO 正确 | ✅ PROTO=88b7 |
| netif_receive_skb RC | ✅ RC=0 |
| rmmod 正常卸载 | ✅ (stage04_exit 修复后) |

## 经验总结

1. **AF_PACKET SOCK_RAW + 特定 ethertype**: kernel 自动处理 Ethernet header，sendto 数据应为纯 payload。如果要手动控制完整 frame，使用 `ETH_P_ALL`。

2. **strtoul hex 解析**: 解析如 "88B7" 这种十六进制字符串，必须显式指定 `base=16`，否则 `strtoul` 会当作 decimal。

3. **模块卸载顺序**: 正确的清理顺序是 `napi_disable` → `netif_tx_disable` → `unregister_netdev` → `dev_remove_pack` → cleanup。任何反向顺序都可能导致死锁。