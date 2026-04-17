# USER_GUIDE

## 快速开始

```bash
cd linux-driver-lab/netdev/stage02_skb_path
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
sudo make unload     # 卸载模块
```

---

## 测试命令

### 直接环回测试

```bash
sudo ./tools/send_stage02_frame nds2 hello
sudo ./tools/recv_stage02_frame nds2
```

### 查看统计

```bash
cat /sys/kernel/debug/netdev_stage02/stats
ip -s link show nds2
```

---

## 模块参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `ifname` | nds2 | net_device 名称 |
| `loop_mode` | copy | 环回模式：copy / clone |

---

## debugfs 统计关键指标

| 指标 | 含义 |
|------|------|
| `tx_packets / rx_packets` | TX/RX 帧数 |
| `copy_built / clone_built` | 两种模式的使用次数 |
| `netif_rx_success / netif_rx_drop` | 注入成功/丢弃次数 |
| `last_tx_proto / last_rx_proto` | 最近一次 ETHERTYPE |

---

## smoke test 要验证的关键事实

1. 接口能正常 up → `ndo_open()` 工作正常
2. send 工具能触发 TX → `ndo_start_xmit()` 路径正常
3. recv 工具能收到包 → 软件环回 + `netif_rx()` 成立
4. `ip -s link` 同时看到 TX / RX 增长

---

## 通过标准

- 能清楚讲出 `skb` 是什么
- 能解释 `copy` 与 `clone` 区别
- 能跑通 TX→RX 软件闭环
- 能解释为什么本阶段先不引入 NAPI/ring/DMA
