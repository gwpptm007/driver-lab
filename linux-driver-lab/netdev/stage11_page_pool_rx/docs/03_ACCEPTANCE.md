# 03_ACCEPTANCE — 验收标准

## 通过标准

| 测试项 | 条件 | 验证方式 |
|--------|------|----------|
| 多队列分发 | 2+ 队列 `tx_submit` **增量** > 0 | `queue_dist_check.sh` |
| 向量调度 | 2+ vectors `handle_count` **增量** >= 1 | `vector_check.sh` |
| 异步链路 | 1+ 队列 `doorbell_to_backend_ns` > 0 | `timeline_check.sh` |
| page_pool 分配 | `pp_alloc` **总量** > 0 | `pp_check.sh` |
| RX 功能 | `ip -s link` 显示 RX packets 增长 | 手动验证 |

### 重要说明

**为什么检查增量而非存量？**

旧版检查 `tx_submit > 0` 或 `handle_count >= 1`（只看病存量），导致：
- 队列之前有过历史流量，即使本轮测试 0 新增也 PASS（假通过）
- 向量之前被 handle 过，即使本轮 0 新增也 PASS（假通过）

**正确方式**：比较 `before/after` 差值，确保本轮测试真正触发了流量/中断。

**关于帧收发统计**：`test_tx`/`test_rx` 不可信，因为 send 工具改用 ETH_P_IP + IP-proto=253 格式后 magic 检查有偏移。实际收发以 `ip -s link` 的 TX/RX packets 增量和 `stats` 中的 `tx_submit`/`rx_consume` 增量为准。

**关于 page_pool 回收**：`pp_recycle` 正常情况下为 0，因为 `build_skb` 成功时 page 由 skb destructor 隐式释放（put_page），不经过 `page_pool_recycle_direct`。只有 `build_skb` 失败时才会触发 `pp_recycle`。`pp_build_skb_fail` 应为 0。

## 运行测试

```bash
cd linux-driver-lab/netdev/stage11_page_pool_rx
./scripts/build.sh
./scripts/smoke.sh
```

## 关键指标解读

### page_pool 统计（page_pool debugfs）

- `pp_alloc`：从 page_pool 分配 page 的次数（每次 `stage11_refill_rx_slot` 成功）
- `pp_recycle`：`put_page` 归 page 回 pool 的次数（正常为 0，失败路径 > 0）
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

### 调试命令

```bash
# 查看所有队列统计（含 page_pool）
sudo cat /sys/kernel/debug/netdev_stage11_soft/stats

# 查看向量映射
sudo cat /sys/kernel/debug/netdev_stage11_soft/vectors

# 查看 timeline
sudo cat /sys/kernel/debug/netdev_stage11_soft/timeline

# 查看队列详细状态
sudo cat /sys/kernel/debug/netdev_stage11_soft/queues

# 查看 page_pool 状态
sudo cat /sys/kernel/debug/netdev_stage11_soft/page_pool
```
