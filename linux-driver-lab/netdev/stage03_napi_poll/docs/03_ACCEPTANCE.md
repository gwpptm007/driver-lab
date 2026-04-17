# ACCEPTANCE

## 验收标准

### 功能层

- [ ] 模块可编译、可加载、可卸载
- [ ] `nds3` 能 `ip link set up`
- [ ] sender / receiver 能工作
- [ ] `rx_mode=direct` 下可以收到环回帧
- [ ] `rx_mode=napi` 下可以收到环回帧

### 观测层

- [ ] debugfs 统计可读
- [ ] `rx_mode=napi` 时：
  - `napi_schedule_count > 0`
  - `napi_poll_count > 0`
  - `napi_complete_count > 0`
  - `pending_enqueued > 0`
  - `pending_drained > 0`
  - `irq_raised > 0`
- [ ] 做 burst 测试时，最好还能观察到：
  - `pending_peak > 1`
  - 或 `napi_budget_exhaust_count > 0`

---

## 统计验收（direct 模式）

- [ ] `direct_inject_count > 0`
- [ ] `napi_inject_count = 0`
- [ ] `pending_enqueued = 0`
- [ ] `irq_raised = 0`
- [ ] `napi_poll_count = 0`

---

## 统计验收（napi 模式）

- [ ] `direct_inject_count = 0`
- [ ] `napi_inject_count > 0`
- [ ] `pending_enqueued > 0`
- [ ] `pending_drained > 0`
- [ ] `irq_raised > 0`
- [ ] `napi_schedule_count > 0`
- [ ] `napi_poll_count > 0`
- [ ] `napi_complete_count > 0`
- [ ] `irq_masked_count = irq_unmasked_count`

---

## 通过标准

> stage03 不是"学会调用 napi API"，而是学会：
> **为什么要把 RX 处理从"每包立刻处理"切到"先排队、后 poll 批处理"。**
