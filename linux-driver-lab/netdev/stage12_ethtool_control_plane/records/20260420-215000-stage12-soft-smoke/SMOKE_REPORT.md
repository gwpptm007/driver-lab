# stage12_soft smoke report

- interface: nds12s
- count: 64
- frame_type: ETH_P_IP (0x0800) + IP-proto=253
- record_dir: 20260420-215000-stage12-soft-smoke
- verdict: PASS

## ethtool 验证

### ethtool -i (驱动信息)
```
driver: netdev_stage12
version: 1.0
bus-info: platform
supports-statistics: yes
```

### ethtool -S (统计)
```
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

## 验证结果

| 测试项 | 结果 | 条件 |
|--------|------|------|
| queue_dist | PASS | 2/2 queues tx_submit delta > 0 |
| vector_check | PASS | 2/2 vectors handle_count delta >= 1 |
| timeline | PASS | 2/2 queues doorbell_to_backend_ns > 0 |
| page_pool | PASS | pp_alloc > 0, pp_build_skb_fail = 0 |

## page_pool 状态

```
q0: pp_alloc=199 posted=127 ready=0
q1: pp_alloc=143 posted=127 ready=0
```

## 关键指标

- 多队列分发: q0=72, q1=16 frames
- Async 链路: doorbell_to_backend_ns q0=25621, q1=172312
- page_pool: 正常工作，无 build_skb_fail