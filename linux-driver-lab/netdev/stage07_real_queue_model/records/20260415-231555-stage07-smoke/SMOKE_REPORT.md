# stage07 smoke test report — 2026-04-15

## 环境
- 测试机: 192.168.65.135 (Ubuntu 22.04, kernel 6.8.0-107-generic)
- 模块: netdev_stage07.ko loaded at 21942.258127
- 设备: nds7 (ring_size=128, napi_weight=32, rx_buf_size=2048)
- 发送: 32帧, ethertype=0x88B7, payload=hello_stage07_queue

## 结果

### TX 路径 ✅
- tx_submit_count=126, tx_complete_count=126 — 完全匹配
- tx_inflight=0, tx_done=0 — 全部回收
- tx_dropped=0, tx_busy=0 — 零丢包
- device_notify_count=126, device_tx_processed=126 — backend 全处理

### RX 路径 ✅
- rx_post_count=254, rx_consume_count=126, rx_refill_count=254 — refill 充足
- rx_packets=126 — 上送协议栈
- rx_dropped=0, rx_truncated=0 — 零异常
- rx_no_posted=0 — backend 未缺 buffer

### NAPI ✅
- irq_count=125, napi_schedule_count=125, napi_poll_count=125 — 一致
- napi_complete_count=125 — 完成正常
- napi_budget_exhaust_count=0 — budget 未耗尽
- napi_work_total=126 — 批处理正常

### Queue Index 验证 ✅
- TX: submit=127, notify=127, complete=127, inflight=0
- RX: post=127, device=127, consume=127, posted=128, ready=0
- 所有 128 个 RX slot 状态为 POSTED — refill 充足

## 结论
**stage07 smoke 测试通过 ✅**
