# USER_GUIDE

## 快速开始

```bash
cd linux-driver-lab/netdev/stage03_napi_poll
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

## 推荐测试顺序

1. 先用 `rx_mode=direct` 跑一遍，确认 stage02 经验在 stage03 里没丢
2. 再切 `rx_mode=napi`
3. 用 burst 发送让 pending queue 真正出现积压
4. 观察 debugfs 中各项统计

---

## 测试命令

### direct 模式

```bash
make build-userspace
make build-module
sudo IFNAME=nds3 RX_MODE=direct make load
sudo ip link set nds3 up
sudo ./tools/recv_stage03_frame nds3 0x88B6 8 5
sudo ./tools/send_stage03_frame nds3 hello_direct 0x88B6 8 0
sudo cat /sys/kernel/debug/netdev_stage03/stats
sudo make unload
```

### napi 模式

```bash
sudo IFNAME=nds3 RX_MODE=napi NAPI_WEIGHT=8 make load
sudo ip link set nds3 up
sudo ./tools/recv_stage03_frame nds3 0x88B6 32 5
sudo ./tools/send_stage03_frame nds3 hello_napi 0x88B6 32 0
sudo cat /sys/kernel/debug/netdev_stage03/stats
```

---

## 模块参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `ifname` | nds3 | net_device 名称 |
| `rx_mode` | napi | 注入模式：direct / napi |
| `napi_weight` | 16 | poll 每次最多处理的包数 |
| `loop_mode` | copy | skb 构造模式：copy / clone |

---

## debugfs 统计关键指标

| 指标 | 含义 |
|------|------|
| `direct_inject_count` vs `napi_inject_count` | 区分两种注入路径 |
| `pending_peak` | 队列最大积压深度，>1 说明出现过积压 |
| `napi_budget_exhaust_count` | budget 耗尽次数，>0 说明 poll 没清空队列 |
| `irq_masked_count` vs `irq_unmasked_count` | 中断抑制次数，应该匹配 |
| `napi_schedule_count` vs `napi_complete_count` | schedule 和 complete 应该匹配 |

---

## smoke test 预期（napi 模式）

```
napi_inject_count > 0
pending_enqueued > 0
pending_drained > 0
irq_raised > 0
napi_schedule_count > 0
napi_poll_count > 0
napi_complete_count > 0
```

---

## 注意事项

### 不要拿 `ping -f localhost` 作为验收

因为 `localhost` 走的是 loopback，不会穿过你这个教学网卡的真实路径。

正确做法：对你的 `nds3` 接口发 burst，看你自己的 sender / receiver。

### budget_exhausted == 0 不代表 NAPI 没工作

只说明：当前 burst 不够大，或 `napi_weight` 设得较大，或 poll 很快把 queue 清空了。

真正关键的是：schedule / poll / complete 是否出现，pending queue 是否真实参与过数据路径。
