# 04. 测试与验收

## 1. 最小验收链

```bash
make report
make build-userspace
make build-module
sudo make load
sudo make smoke
sudo make unload
```

## 2. smoke 里要验证的关键事实

### 事实1：接口能正常 up
说明 `ndo_open()` 工作正常。

### 事实2：send 工具能触发 TX
说明 `ndo_start_xmit()` 路径工作正常。

### 事实3：recv 工具能收到包
说明软件环回 + `netif_rx()` 注入成立。

### 事实4：ip -s link 同时看到 TX / RX 增长
说明这不是“只统计 TX 的假闭环”。

### 事实5：debugfs 能解释 loop_mode / rx injection
说明 stage02 的教学统计是自洽的。

## 3. 建议重点查看的统计项

### 标准统计
- `tx_packets`
- `tx_bytes`
- `rx_packets`
- `rx_bytes`
- `tx_dropped`
- `rx_dropped`

### 私有 debugfs 统计
- `loop_mode`
- `loop_injected`
- `copy_built`
- `clone_built`
- `netif_rx_success`
- `netif_rx_drop`
- `last_tx_len/proto`
- `last_rx_len/proto`

## 4. 阶段通过标准

- 能清楚讲出 `skb` 是什么
- 能解释 `copy` 与 `clone` 区别
- 能跑通 TX→RX 软件闭环
- 能解释为什么本阶段先不引入 NAPI/ring/DMA
