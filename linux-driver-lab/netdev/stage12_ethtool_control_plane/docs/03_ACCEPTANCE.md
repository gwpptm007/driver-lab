# 03_ACCEPTANCE — stage12 验收标准

## 通过标准

| 测试项 | 条件 | 验证方式 |
|--------|------|----------|
| **ethtool -i** | 显示 `driver: netdev_stage12` | 手动验证 |
| **ethtool -S** | 显示所有统计项 | 手动验证 |
| 多队列分发 | 2+ 队列 `tx_submit` **增量** > 0 | `queue_dist_check.sh` |
| 向量调度 | 2+ vectors `handle_count` **增量** >= 1 | `vector_check.sh` |
| 异步链路 | 1+ 队列 `doorbell_to_backend_ns` > 0 | `timeline_check.sh` |
| page_pool 分配 | `pp_alloc` **总量** > 0 | `pp_check.sh` |

### 重要说明

**为什么检查增量而非存量？**

旧版检查 `tx_submit > 0` 或 `handle_count >= 1`（只看病存量），导致：
- 队列之前有过历史流量，即使本轮测试 0 新增也 PASS（假通过）
- 向量之前被 handle 过，即使本轮 0 新增也 PASS（假通过）

**正确方式**：比较 `before/after` 差值，确保本轮测试真正触发了流量/中断。

**关于 page_pool 回收**：`pp_recycle` 正常情况下为 0，因为 `build_skb` 成功时 page 由 skb destructor 隐式释放（put_page），不经过 `page_pool_recycle_direct`。只有 `build_skb` 失败时才会触发 `pp_recycle`。`pp_build_skb_fail` 应为 0。

## ethtool 验证方法

### ethtool -i (驱动信息)
```bash
$ ethtool -i nds12s
driver: netdev_stage12
version: 1.0
bus-info: platform
supports-statistics: yes
```

### ethtool -S (统计导出)
```bash
$ ethtool -S nds12s
NIC statistics:
     tx_packets: 88
     tx_bytes: 14148
     tx_submit_count: 88
     tx_complete_count: 88
     rx_packets: 88
     rx_bytes: 14148
     rx_consume_count: 88
     rx_page_alloc: 342
     rx_build_skb_fail: 0
```

### ethtool -G (ring 参数)
```bash
$ ethtool -G nds12s
Ring parameters for nds12s:
RX currently: 128
RX max: 128
TX currently: 128
TX max: 128
```

### ethtool -L (channel 数)
```bash
$ ethtool -L nds12s
Channel parameters for nds12s:
RX: 2
TX: 2
Combined: 0
```

## 运行测试

```bash
cd linux-driver-lab/netdev/stage12_ethtool_control_plane
./scripts/build.sh
./scripts/smoke.sh
```

## 关键指标解读

### ethtool stats 字段说明

| 字段 | 含义 |
|------|------|
| `tx_packets` | TX 完成交付的 packet 数 |
| `tx_bytes` | TX 交付的字节数 |
| `tx_submit_count` | TX 提交到驱动的次数 |
| `tx_complete_count` | TX complete 处理次数 |
| `rx_packets` | RX 交付给协议栈的 packet 数 |
| `rx_consume_count` | RX consume 处理次数 |
| `rx_page_alloc` | page_pool 分配 page 次数 |
| `rx_build_skb_fail` | build_skb 失败次数（应为 0） |

### page_pool 统计（debugfs page_pool）

- `pp_alloc`：从 page_pool 分配 page 的次数
- `pp_recycle`：`put_page` 归 page 回 pool 的次数（正常为 0）
- `pp_build_skb_fail`：`build_skb()` 失败次数（应为 0）

### timeline 数据（timeline_check）

- `submit_to_doorbell_ns`：`ndo_start_xmit` 到 `mark_doorbell` 的延迟（~100ns）
- `doorbell_to_backend_ns`：`mark_doorbell` 到 backend workfn 执行的延迟（~10-30µs）
- `backend_to_irq_ns`：backend 完成到 irq_workfn 执行的延迟（~10µs）
- `irq_to_poll_ns`：irq_workfn 到 napi_poll 执行的延迟（~3µs）

### vector 数据（vector_check）

- `raise`：向量的 `raise_irq` 被调用次数
- `handle`：`irq_workfn` 实际执行的次数
- `schedule`：`napi_schedule` 被调用的总次数

## 调试命令

```bash
# ethtool 驱动信息
ethtool -i nds12s

# ethtool 统计
ethtool -S nds12s

# ethtool ring 参数
ethtool -G nds12s

# ethtool channel 数
ethtool -L nds12s

# 查看所有队列统计（含 page_pool）
sudo cat /sys/kernel/debug/netdev_stage12_soft/stats

# 查看向量映射
sudo cat /sys/kernel/debug/netdev_stage12_soft/vectors

# 查看 timeline
sudo cat /sys/kernel/debug/netdev_stage12_soft/timeline

# 查看 page_pool 状态
sudo cat /sys/kernel/debug/netdev_stage12_soft/page_pool
```