# USER_GUIDE

## 快速开始

```bash
cd linux-driver-lab/netdev/stage04_ring_dma
make report
make build-userspace
make build-module
sudo make load
sudo make smoke
sudo make unload
```

---

## 常用命令

```bash
make report          # 主机环境检测
make build-userspace # 编译用户态工具
make build-module    # 编译内核模块
sudo make load       # 加载模块
sudo make smoke      # 运行 smoke test
sudo make unload    # 卸载模块
```

---

## 手动验证步骤

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

```bash
sudo insmod output/netdev_stage04.ko ifname=nds4 ring_size=8 napi_weight=2 rx_buf_size=512
```

然后再 burst 发包，看：

- `napi_budget_exhaust_count`
- `rx_ring_done`
- `rx_pending_peak`
- `rx_no_desc_drop`

---

## 模块参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `ifname` | nds4 | net_device 名称 |
| `ring_size` | 64 | TX/RX ring 深度 |
| `napi_weight` | 16 | poll 每次最多处理的包数 |
| `rx_buf_size` | 2048 | 预分配 RX buffer 大小 |

---

## 调试三板斧

```bash
# 1. dmesg 看 TX/RX 打印
sudo dmesg | grep stage04

# 2. debugfs stats 看计数
cat /sys/kernel/debug/netdev_stage04/stats

# 3. debugfs rings 看 ring 状态
cat /sys/kernel/debug/netdev_stage04/rings
```

---

## smoke test 预期输出

```
[stage04] TX RXIDX=21 SKBLEN=20 CPYLEN=20 ETH=88b7
[stage04] POLL IDX=21 LEN=20 PROTO=88b7 RC=0
```

| 字段 | 含义 | 预期值 |
|------|------|--------|
| `ETH` | TX 路径 skb->data[14:15]（ethertype） | `88b7` |
| `PROTO` | RX 路径 eth_type_trans 解析结果 | `88b7` |
| `RC` | netif_receive_skb 返回值（0=成功） | `0` |

---

## 验收时最应该看什么

1. `debugfs` 里 refill 计数是否增加
2. ring dump 里 slot 是否从 `DONE -> POSTED` 循环
3. 高 burst 下是否出现 `rx_no_desc_drop`
4. budget 小于 burst 时，是否能观察到分批 drain
