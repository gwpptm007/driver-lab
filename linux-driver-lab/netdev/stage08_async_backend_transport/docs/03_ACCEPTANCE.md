# ACCEPTANCE & TASKS

## R1：阶段已建仓

- [x] 新建 stage08 目录
- [x] 文档目录齐（3 个：01_STAGE_OVERVIEW, 02_USER_GUIDE, 03_ACCEPTANCE）
- [x] 脚本入口齐（build/run/smoke/stats_check/timeline_check/trace_smoke）
- [x] driver 代码骨架齐

## R2：异步主链成立

- [x] xmit -> queue submit
- [x] queue doorbell -> queue_work()
- [x] backend worker -> TX process
- [x] backend worker -> RX produce
- [x] backend worker -> raise irq
- [x] napi poll -> complete tx
- [x] napi poll -> consume rx
- [x] napi poll -> refill rx

## R3：观测成立

- [x] stats 可读
- [x] queues 可读
- [x] timeline 可读
- [x] 能看出 submit/doorbell/backend/irq/poll 的先后关系

## R4：第一轮测试验收

- [ ] 在测试机上 build 成功
- [ ] 模块 load 成功
- [ ] smoke 跑通
- [ ] records 归档完整
- [ ] stage07 vs stage08 差异可解释

---

## 通过标准

> **不追求复杂功能，只要求前后端边界、doorbell、异步 completion 与 timeline 观测都成立。**

### 验收检查单

```bash
# 1. 检查 timeline 时间戳非零（证明异步链路通了）
cat /sys/kernel/debug/netdev_stage08/timeline
# 应看到 last_submit_ns, last_doorbell_ns, last_backend_wakeup_ns 等都有值

# 2. 检查 backend 处理了数据
cat /sys/kernel/debug/netdev_stage08/stats | grep backend
# backend_run_count > 0
# backend_tx_processed > 0
# backend_rx_produced > 0

# 3. 检查 NAPI 完成了收包
cat /sys/kernel/debug/netdev_stage08/stats | grep -E "rx_consume|tx_complete"
# rx_consume_count > 0
# tx_complete_count > 0

# 4. 对比 stage07：异步时间线 delta 应 > 0（当 backend_delay_us > 0 时）
```

---

## 后续推进项

- [ ] 在测试机上完成第一轮编译/加载/烟测
- [ ] 根据测试结果修正 driver 主逻辑
- [ ] 增加 timeline delta 检查
- [ ] 增加 stage07 vs stage08 对比报告
- [ ] 评估是否需要 backend delay profile
- [ ] 评估是否需要 backend batch size profile


## v2 收口重点

- [ ] `recv.txt` 非空，且包含 `received ... matched_magic=32`
- [ ] `stats_check.sh` 基于 before/after 差分返回 PASS
- [ ] `timeline_check.sh` 证明 `doorbell -> backend` 为异步（delta > 0）
- [ ] 若 `debugfs/test_stats` 存在，则 4 个 test counter 必须严格等于 32
- [ ] 测试结束后 `tx_inflight / tx_done / rx_ready / doorbell_pending / backend_running` 必须全部归零

### v2 通过定义

stage08 v2 的通过，不再只是“看起来收发正常”，而是要同时满足：

1. sender 摘要存在；
2. receiver 摘要存在且 `matched_magic == 32`；
3. stats delta 证明本次 smoke 触发了 submit/doorbell/backend/irq/napi 链；
4. timeline 明确看到 `doorbell -> backend` 异步延迟；
5. 队列 end-state 干净。
