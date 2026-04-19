# 03_ACCEPTANCE — 验收标准

## 通过标准

| 测试项 | 条件 | 验证方式 |
|--------|------|----------|
| 多队列分发 | 2+ 队列 `tx_submit` **增量** > 0 | `queue_dist_check.sh` |
| 向量调度 | 2+ vectors `handle_count` **增量** >= 1 | `vector_check.sh` |
| 异步链路 | 1+ 队列 `doorbell_to_backend_ns` > 0 | `timeline_check.sh` |
| 帧收发 | `ip -s link` 显示 TX/RX packets 增长 | 手动验证 |

### 重要说明

**为什么检查增量而非总量？**

旧版检查 `tx_submit > 0` 或 `handle_count >= 1`（只看病存量），导致：
- 队列之前有过历史流量，即使本轮测试 0 新增也 PASS（假通过）
- 向量之前被 handle 过，即使本轮 0 新增也 PASS（假通过）

**正确方式**：比较 `before/after` 差值，确保本轮测试真正触发了流量/中断。

**关于帧收发统计**：`test_tx`/`test_rx` 已不可信，因为 send 工具改用 ETH_P_IP + IP-proto=253 格式后，magic 检查逻辑不再匹配。实际收发以 `ip -s link` 的 TX/RX packets 增量和 `stats` 中的 `tx_submit`/`rx_consume` 增量为准。

## 运行测试

```bash
cd linux-driver-lab/netdev/stage10_msix_per_queue_irq/soft
./scripts/smoke.sh
```

## 关键指标解读

### timeline 数据（timeline_check）

- `submit_to_doorbell_ns`：`ndo_start_xmit` 到 `mark_doorbell` 的延迟（~100ns）
- `doorbell_to_backend_ns`：`mark_doorbell` 到 backend workfn 执行的延迟（~10-30µs）
- `backend_to_irq_ns`：backend 完成到 irq_workfn 执行的延迟（~10µs）
- `irq_to_poll_ns`：irq_workfn 到 napi_poll 执行的延迟（~3µs）

### vector 数据（vector_check）

- `raise`：向量的 `raise_irq` 被调用次数
- `handle`：`irq_workfn` 实际执行的次数（napi_schedule 成功次数）
- `schedule`：`napi_schedule` 被调用的总次数

如果 `raise > handle`，说明某些向量中断被 masked 了（`irq_masked` 计数）。

### 调试命令

```bash
# 查看所有队列统计
sudo cat /sys/kernel/debug/netdev_stage10_soft/stats

# 查看向量映射
sudo cat /sys/kernel/debug/netdev_stage10_soft/vectors

# 查看 timeline
sudo cat /sys/kernel/debug/netdev_stage10_soft/timeline

# 查看队列详细状态
sudo cat /sys/kernel/debug/netdev_stage10_soft/queues
```
