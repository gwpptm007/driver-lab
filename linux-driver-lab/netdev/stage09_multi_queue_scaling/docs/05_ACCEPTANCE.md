# 05_ACCEPTANCE — 通过标准

## v1 通过标准（5 项）

### 1. 编译通过

```
netdev_stage09.ko 编译成功
send_stage09_frame 和 recv_stage09_frame 编译成功
```

验证：`./build.sh` 无 error。

### 2. 模块加载成功

```
insmod netdev_stage09.ko 成功
ifconfig nds9 up 成功
设备出现在 ifconfig 输出中
```

验证：`lsmod | grep netdev_stage09` + `cat /sys/kernel/debug/netdev_stage09/stats`

### 3. 多队列 TX 活跃（核心指标）

```
至少两个 queue 出现 tx_submit / tx_complete 计数 > 0
```

**为什么 >= 2 个队列？**
- stage09 的核心价值是验证"流量被分散到多个队列"
- 如果只有一个队列活跃，说明分发策略有问题（退化为单队列）
- 多个 flow 并发发送才能触发多队列活跃

验证：`./scripts/queue_dist_check.sh records/<latest>` 输出 `PASS: 2+ active queues`

### 4. 异步链路成立（核心指标）

```
doorbell_to_backend_ns > 0（至少一个 queue）
```

**为什么 > 0 证明异步？**
- `doorbell_to_backend_ns` = `backend_wakeup_ns - doorbell_ns`
- 如果 == 0：backend 是在 doorbell 调用栈上直接运行（**同步**）
- 如果 > 0：backend 是被 workqueue schedule 的（**异步**）

验证：`./scripts/timeline_check.sh records/<latest>` 输出 `PASS: async chain verified`

### 5. 资源回收干净（稳定性指标）

```
测试结束后所有队列的以下计数器都回到 0：
tx_inflight / tx_done / rx_posted / rx_ready /
doorbell_pending / backend_running
```

**为什么这很重要？**
- 非零值说明资源泄漏或处理未完成
- `inflight > 0` 且不再变化 = TX 完成路径有问题
- `backend_running=true` 持续不变 = backend 死锁

验证：`cat /sys/kernel/debug/netdev_stage09/queues` 所有队列计数器 == 0

---

## 通过标准速查表

| # | 检查项 | 验证方法 | 期望结果 |
|---|--------|----------|----------|
| 1 | 编译通过 | `./build.sh` | 无 error |
| 2 | 模块加载 | `lsmod && ifconfig nds9` | 设备存在 |
| 3 | 多队列活跃 | `queue_dist_check.sh` | >= 2 队列有 tx_submit |
| 4 | 异步链路 | `timeline_check.sh` | >= 1 队列 doorbell_to_backend > 0 |
| 5 | 资源回收 | `debugfs/queues` | 所有队列计数器 == 0 |

---

## 典型失败分析

### queue_dist_check.sh 失败：只有 1 个队列活跃

**原因**：smoke test 只发送了 1 个 flow（所有帧 hash 到同一队列）
**解决**：使用多个并发 flow 发送，或关闭 hash 观察 rr_counter 递增

### timeline_check.sh 失败：doorbell_to_backend == 0

**原因**：`backend_delay_us=0` 且 ring 很小，backend 处理太快（同步执行）
**解决**：设置 `backend_delay_us=100` 增加异步延迟
