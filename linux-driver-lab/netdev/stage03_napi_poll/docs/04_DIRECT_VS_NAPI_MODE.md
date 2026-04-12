# 04_DIRECT_VS_NAPI_MODE

## 为什么保留两种模式

stage03 不是要“彻底抛弃 stage02”，而是要让你有一个可对照的学习支点。

因此驱动保留了两个运行模式：

- `rx_mode=direct`
- `rx_mode=napi`

## direct 模式

### 路径
```text
start_xmit -> build_rx_skb -> netif_rx
```

### 优点
- 路径最短
- 最利于看 `skb` 的构造与注入
- 适合作为 stage02 经验的延续

### 局限
- 没有排队
- 没有 budget
- 看不到 poll / complete / irq suppression

## napi 模式

### 路径
```text
start_xmit
  -> build_rx_skb
  -> enqueue
  -> raise irq
  -> napi poll
  -> netif_receive_skb
```

### 优点
- 能观察真正的 NAPI 教学语义
- 能看到批处理与 queue depth
- 能对比 direct 与 napi 的行为差异

### 局限
- 仍然是教学模型
- 还没有真实 ring / DMA

## 建议的测试顺序

1. 先用 `rx_mode=direct` 跑一遍，确认 stage02 经验在 stage03 里没丢
2. 再切 `rx_mode=napi`
3. 用 burst 发送让 pending queue 真正出现积压
4. 观察 debugfs 中：
   - `napi_schedule_count`
   - `napi_poll_count`
   - `napi_budget_exhaust_count`
   - `pending_peak`
   - `irq_masked_count / irq_unmasked_count`

## 一个重要提醒

不要拿 `ping -f localhost` 作为 stage03 的验收例子。

因为 `localhost` 走的是 loopback，不会穿过你这个教学网卡的真实路径。

正确做法应该是：
- 对你的 `nds3` 接口发 burst
- 看你自己的 sender / receiver
- 看你自己的 debugfs 统计
