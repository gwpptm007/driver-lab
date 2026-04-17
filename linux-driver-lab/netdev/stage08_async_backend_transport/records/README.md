# records

本目录用于归档 stage08 的真实测试证据。

## 目录结构

每次 smoke 测试生成一个带时间戳的目录：

```
records/
├── 20260417-232819-stage08-smoke/   # 2026-04-17 23:28:19 smoke 测试
└── 20260417-232810-stage08-smoke/   # 更早一次测试
```

每个 smoke 目录包含：

| 文件 | 说明 |
|------|------|
| `send.txt` | 发送工具输出（发送了多少帧） |
| `recv.txt` | 接收工具输出（接收了多少帧） |
| `debugfs_stats_before.txt` | 测试前统计 |
| `debugfs_stats_after.txt` | 测试后统计 |
| `debugfs_timeline_before.txt` | 测试前 timeline |
| `debugfs_timeline_after.txt` | 测试后 timeline |
| `debugfs_queues_after.txt` | 测试后 ring 队列状态 |
| `ip_link_before.txt` | 测试前 `ip link` 输出 |
| `ip_link_after.txt` | 测试后 `ip link` 输出 |
| `dmesg_tail.txt` | 内核日志 |
| `SMOKE_REPORT.md` | 本次 smoke 报告 |

---

## 验证方法

### 快速验证命令

```bash
# 1. 编译
./scripts/build.sh

# 2. 加载模块
./scripts/run.sh reload

# 3. 执行 smoke
sudo ./scripts/smoke.sh

# 4. 查看统计
./scripts/stats_check.sh

# 5. 查看 timeline
./scripts/timeline_check.sh
```

### 验收通过标准

**TX 路径（发送 32 帧，累积 51）：**
- [x] `tx_submit_count` > 0（submit 被调用）
- [x] `doorbell_count` > 0（doorbell 被调用）
- [x] `tx_complete_count` > 0（TX 完成）
- [x] `backend_tx_processed` > 0（backend 处理了 TX）
- [x] `tx_packets == tx_complete_count`（无丢包）

**RX 路径（接收 51 帧）：**
- [x] `rx_consume_count` > 0（RX 被消费）
- [x] `rx_packets > 0`（收到帧）
- [x] `rx_refill_count > 0`（ refill 发生）
- [x] `rx_posted == 128`（RX buffer 池充足）

**异步 backend 模型：**
- [x] `backend_schedule_count` > 0（backend 被调度）
- [x] `backend_run_count` > 0（backend 实际运行）
- [x] `delta_doorbell_to_backend_ns > 0`（异步延迟存在）

**NAPI 中断抑制：**
- [x] `irq_count == napi_poll_count`（每次 irq 触发一次 poll）
- [x] `napi_budget_exhaust_count == 0`（budget 够用）

**无错误：**
- [x] `tx_dropped == 0`
- [x] `rx_dropped == 0`
- [x] `tx_dma_map_fail == 0`
- [x] `rx_dma_map_fail == 0`

---

## 20260417-232819-stage08-smoke 验证举证

### 基本信息
- 测试时间：2026-04-17 23:28:19
- 测试接口：nds8
- 测试帧数：32 帧/次
- 累计发送：51 帧

### TX 路径举证

```
tx_submit_count=51       # 51 帧被 submit
doorbell_count=51        # 51 次 doorbell
backend_schedule_count=51 # 51 次 backend 调度
backend_run_count=22     # backend 实际运行 22 次（批处理）
backend_tx_processed=51  # 51 帧被 backend 处理
tx_complete_count=51    # 51 帧完成
tx_dropped=0             # 无丢包
```

**判断**：TX 异步模型工作正常，backend 批处理（每批 ~2-3 帧）生效。

### RX 路径举证

```
rx_consume_count=51       # 51 帧被消费
rx_packets=51             # 协议栈收到 51 帧
rx_refill_count=179      # 179 次 refill（RX buffer 池补充）
rx_posted=128             # 128 个 buffer 处于 posted 状态
rx_dropped=0              # 无丢包
```

**判断**：RX 路径环形buffer工作正常，refill 机制持续补充 buffer。

### Timeline 举证

```
delta_submit_to_doorbell_ns=140        # submit 到 doorbell：140ns（同步）
delta_doorbell_to_backend_ns=45524      # doorbell 到 backend 唤醒：45μs（异步）
delta_backend_to_irq_ns=70             # backend 完成到 irq：70ns
delta_irq_to_poll_ns=4839              # irq 到 poll 开始：4.8μs
delta_irq_to_poll_ns=4839              # irq 到 poll 开始：4.8μs
```

**判断**：
1. doorbell 是同步触发的（140ns）
2. backend 是异步执行的（~45μs 延迟）
3. irq → poll 路径存在（中断抑制生效）

### NAPI 举证

```
irq_count=22
napi_schedule_count=22
napi_poll_count=22
napi_complete_count=22
napi_budget_exhaust_count=0
napi_work_total=51
```

**判断**：每次 irq 触发一次 poll，22 次 poll 处理了 51 帧（平均每批 ~2.3 帧），budget 充足。

### Ring 状态举证

```
TX submit=51 notify=51 complete=51 inflight=0 done=0
  tx[0~15] desc=FREE slot=FREE  # 所有 slot 归还

RX post=51 device=51 consume=51 posted=128 ready=0
  rx[0~15] desc=POSTED slot=POSTED  # 16 个 buffer 处于 posted 状态
```

**判断**：ring 管理正常，所有 TX slot 用后归还，RX buffer 池充足。

### dmesg 关键日志

```
[stage08] nds8: netdev_stage08: loaded, ifname=nds8
[stage08] nds8: ndo_open: open_count=1
[stage08] nds8: ndo_stop: stop_count=0
[stage08] nds8: ndo_start_xmit: submit skb len=62
[stage08] nds8: doorbell_hit: doorbell=1 pending=0
[stage08] nds8: backend_worker: wakeup scheduled
[stage08] nds8: backend_worker: run: tx=32 batch=32
[stage08] nds8: backend_worker: done: tx=32 rx=32 irq_raised=1
[stage08] nds8: napi_poll: napi_complete work=3 budget=64
```

---

## 一句话总结

stage08 前后端异步模型验证通过：

- **TX**：submit → doorbell（同步）→ backend worker（异步批处理）→ irq → poll → complete
- **RX**：backend 产生 → irq → poll → consume → refill
- **关键延迟**：doorbell 到 backend 唤醒 ~45μs，证明异步模型已生效
- **NAPI**：irq/poll 1:1 对应，中断抑制生效
- **无丢包、无错误**
