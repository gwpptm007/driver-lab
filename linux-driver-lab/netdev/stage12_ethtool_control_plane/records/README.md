# Stage12 Records

## 测试记录

| 时间 | 目录 | 内容 |
|------|------|------|
| 2026-04-20 21:50 | 20260420-215000-stage12-soft-smoke | smoke test + queue_dist + vector + timeline |
| 2026-04-20 22:20 | 20260420-222000-stage12-ethtool-check | ethtool -i/-S/-G/-L 验证 |

## 测试通过标准

1. **ethtool -i** 显示 `driver: netdev_stage12`
2. **ethtool -S** 显示所有统计项 (tx_packets, rx_consume_count 等)
3. **ethtool -G** 显示 ring 参数
4. **ethtool -L** 显示 channel 数
5. **smoke test** 收发包正常