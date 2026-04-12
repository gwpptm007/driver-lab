# 04_TEST_AND_ACCEPTANCE

## 推荐验收步骤

```bash
cd linux-driver-lab/netdev/stage04_ring_dma
make report
make build-userspace
make build-module
sudo make load
sudo make smoke
sudo make unload
```

## 建议手动验证

### 1. 最小 smoke

```bash
sudo ip link set nds4 up
sudo ./tools/recv_stage04_frame nds4 0x88B7 8 5
sudo ./tools/send_stage04_frame nds4 hello_stage04 0x88B7 8 0
```

### 2. 看 debugfs stats

```bash
sudo cat /sys/kernel/debug/netdev_stage04/stats
sudo cat /sys/kernel/debug/netdev_stage04/rings
```

### 3. 调小 budget 或 ring 深度做观察

例如：

```bash
sudo insmod output/netdev_stage04.ko ifname=nds4 ring_size=8 napi_weight=2 rx_buf_size=512
```

然后再 burst 发包，看：

- `napi_budget_exhaust_count`
- `rx_ring_done`
- `rx_pending_peak`
- `rx_no_desc_drop`

## 通过口径

### 基本通过

- sender / receiver 能在 nds4 上看到闭环
- `debugfs` 能看到 TX/RX、DMA、refill 计数
- poll 能 drain ring

### 进阶通过

- ring dump 能解释 ownership/state 变化
- ring_size / napi_weight 变化会影响行为
- 可以用 burst 看到预算耗尽或 ring 紧张的现象
