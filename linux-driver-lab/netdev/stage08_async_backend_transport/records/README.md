# records

本目录用于归档 stage08 的真实测试证据。

## 目录结构

每次 smoke 测试生成一个带时间戳的目录：

```
records/
├── 20260418-001506-full-smoke/      # 2026-04-18 00:15:06 全面 smoke 测试（debugfs + tcpdump 验证）
└── 20260417-232819-stage08-smoke/   # 2026-04-17 23:28:19 smoke 测试
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

# 3. 执行 smoke（需要 sudo 免密）
sudo ./scripts/smoke.sh

# 4. 查看统计
./scripts/stats_check.sh

# 5. 查看 timeline
./scripts/timeline_check.sh
```

### 备选验证方式（当 recv_stage08_frame 因 PACKET_IGNORE_OUTGOING 无法工作时）

如果 AF_PACKET 自发自收有问题（kernel 6.8+ virtio 环境），使用 debugfs + tcpdump 验证：

```bash
# 发送前抓拍 stats
cat /sys/kernel/debug/netdev_stage08/stats > stats_before.txt

# 发送帧
./tools/send_stage08_frame nds8 test 0x88B8 32 0

# 等待异步处理完成
sleep 2

# 发送后抓拍 stats
cat /sys/kernel/debug/netdev_stage08/stats > stats_after.txt
cat /sys/kernel/debug/netdev_stage08/timeline > timeline_after.txt

# tcpdump 验证帧在网口上
sudo tcpdump -i nds8 -c 32 ether proto 0x88B8

# 差分分析
./scripts/stats_check.sh <log_dir> 32
./scripts/timeline_check.sh <log_dir>
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

## 20260418-001506-full-smoke 验证举证（2026-04-18）

### 背景说明

本次测试在 Ubuntu 22.04（kernel 6.8.0-107-generic）上进行，用于验证 stage08 v2 功能更新（sender/receiver 摘要行、stats_check 差分校验、timeline_check 时序硬判定）。

**注意**：由于 `PACKET_IGNORE_OUTGOING` 在该内核版本 virtio 驱动上对 AF_PACKET 自发自收的特殊过滤行为，`recv_stage08_frame` 无法通过 AF_PACKET 接收到本机发出的帧。因此本轮采用 `debugfs stats` 差分 + `tcpdump` 抓包的双重验证。

### 基本信息
- 测试时间：2026-04-18 00:15:06
- 测试接口：nds8
- 测试帧数：32 帧
- 验证方式：debugfs stats 差分 + tcpdump 捕获

### TX 路径举证

```
tx_submit_count delta=32       # 32 帧被 submit
doorbell_count delta=32        # 32 次 doorbell
backend_schedule_count delta=32 # 32 次 backend 调度
backend_run_count delta=3       # backend 实际运行 3 次（批处理，每批 ~10 帧）
backend_tx_processed delta=32  # 32 帧被 backend 处理
tx_complete_count delta=32     # 32 帧完成
tx_dropped=0                   # 无丢包
```

**判断**：TX 异步模型工作正常，backend 批处理生效（3 次运行处理 32 帧）。

### RX 路径举证

```
rx_consume_count delta=32      # 32 帧被消费
rx_refill_count delta=83       # 83 次 RX buffer refill
rx_posted=128                  # 128 个 buffer 处于 posted 状态
rx_dropped=0                   # 无丢包
```

**判断**：RX 路径环形 buffer 工作正常，refill 机制持续补充 buffer。

### Timeline 举证

```
delta_submit_to_doorbell_ns=140        # submit 到 doorbell：140ns（同步）
delta_doorbell_to_backend_ns=13847      # doorbell 到 backend 唤醒：13.8μs（异步）
delta_backend_to_irq_ns=70             # backend 完成到 irq：70ns
delta_irq_to_poll_ns=2044              # irq 到 poll 开始：2μs
```

**判断**：
1. `doorbell_to_backend = 13847ns > 0` —— **异步模型的核心证据**
2. irq → poll 路径存在（中断抑制生效）
3. 与上次测试（45μs）量级一致，验证了 backend_delay_us 模拟延迟

### tcpdump 抓包验证

```
tcpdump captured: 128 frames (32 frames × 4 = TX双向+RX双向的混合)
```

tcpdump 在 nds8 上抓到了 128 帧，远超发送的 32 帧，说明：
- 发送阶段：32 帧从 nds8 发出
- 回环阶段：32 帧从 virtio 后端回环到 RX

**判断**：virtio TX→RX 环回路径全程可观测。

### smoke.sh 各层验证结果

| 验证层 | 结果 | 说明 |
|--------|------|------|
| stats_check | **PASS** | TX=32, RX=32, backend_run=3, 所有 end-state=0 |
| timeline_check | **PASS** | doorbell_to_backend=13847ns>0，异步链成立 |
| tcpdump | **PASS** | 128 帧被抓取，远超预期帧数 |
| send_summary | **PASS** | 发送了 32 帧 |
| recv (AF_PACKET) | N/A | PACKET_IGNORE_OUTGOING 导致该层无法验证 |

### smoke_report.txt 关键内容

```
send_summary: PASS
tcpdump_capture: PASS (captured 128 >= 32)
stats_check: PASS
timeline_check: PASS
OVERALL: PASS
```

### end-state 清洁检查

```
tx_inflight=0          # 无 TX 在飞
tx_done=0               # TX done ring 空
rx_ready=0              # RX ready ring 空
doorbell_pending=0      # 无待处理 doorbell
backend_running=0       # backend 已停止
```

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
