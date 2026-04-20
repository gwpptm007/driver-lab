# 03_ACCEPTANCE — stage13 验收标准

## 通过标准

| 测试项 | 条件 | 验证方式 |
|--------|------|----------|
| **ethtool -k** | 显示 `rx-checksum`, `tx-checksum`, `sg`, `gso`, `gro` | 手动验证 |
| **ethtool -K gro off** | 关闭后 `rx_gro_packets` 不增长 | `offload_experiment.sh` |
| **ethtool -K gro on** | 开启后 `rx_gro_packets` 增长 | `offload_experiment.sh` |
| **ndo_set_features** | 每次 `ethtool -K` 触发 `feature_set_count++` | `offload_experiment.sh` |
| smoke test PASS | 收发包正常 | `smoke.sh` |

---

## offload 验证方法

### ethtool -k (offload 能力查询)

```bash
$ ethtool -k nds13s
rx-checksumming: on
tx-checksumming: on
scatter-gather: on
tcp-segmentation-offload: off
udp-fragmentation-offload: off
generic-segmentation-offload: on
generic-receive-offload: on
```

### ethtool -K (offload 开关)

```bash
# 关闭 GRO
ethtool -K nds13s gro off

# 开启 GRO
ethtool -K nds13s gro on

# 查看 feature 状态
ethtool -k nds13s | grep gro
```

### ethtool -S (offload 统计)

```bash
$ ethtool -S nds13s
     tx_packets: 88
     tx_bytes: 14148
     tx_csum_partial: 0
     tx_gso_packets: 0
     rx_gro_packets: 88
     feature_set_count: 2
```

### debugfs offload

```bash
$ cat /sys/kernel/debug/netdev_stage13_soft/offload
features=0x1c03a rx_csum=1 tx_csum=1 sg=1 gso_sw=1 gro=1 last_features=0x1c03a
```

---

## 运行测试

```bash
cd linux-driver-lab/netdev/stage13_offload_basics
./scripts/build.sh
./scripts/run.sh reload

# 基础 offload 检查
./scripts/offload_check.sh

# GRO 开关实验（核心验证）
./scripts/offload_experiment.sh

# smoke test
./scripts/smoke.sh
```

---

## 关键指标解读

### offload 统计字段说明

| 字段 | 含义 |
|------|------|
| `tx_csum_partial` | TX checksum partial 次数（驱动负责计算） |
| `tx_gso_packets` | TX GSO 包数（协议栈分段） |
| `rx_gro_packets` | RX GRO 收包数（走 GRO 路径） |
| `feature_set_count` | feature 协商次数（每次 ethtool -K 触发） |

### ip_summed 状态

| 状态 | 驱动行为 | 协议栈行为 |
|------|---------|-----------|
| `CHECKSUM_NONE` | 不设置 | 自己计算 |
| `CHECKSUM_UNNECESSARY` | 设置（驱动保证） | 不重复算 |
| `CHECKSUM_PARTIAL` | 设置（部分算） | 需要完整计算 |

### GRO 路径 vs 普通路径

| 路径 | 函数 | 行为 |
|------|------|------|
| GRO | `napi_gro_receive()` | 合并同类包后上送 |
| 普通 | `netif_receive_skb()` | 逐包上送 |

---

## 调试命令

```bash
# offload 能力查询
ethtool -k nds13s

# offload 开关
ethtool -K nds13s gro off
ethtool -K nds13s gro on

# offload 统计
ethtool -S nds13s

# debugfs offload 状态
cat /sys/kernel/debug/netdev_stage13_soft/offload

# 查看所有队列统计
sudo cat /sys/kernel/debug/netdev_stage13_soft/stats

# 查看 timeline
sudo cat /sys/kernel/debug/netdev_stage13_soft/timeline
```
