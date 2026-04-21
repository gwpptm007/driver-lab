# Stage13 Records

## 测试记录

| 时间 | 目录 | 内容 |
|------|------|------|
| 2026-04-20 23:24 | 20260420-232454-stage13-offload-test | offload_check + offload_experiment + smoke test |

## 测试通过标准

1. **ethtool -k** 显示 rx-checksum, tx-checksum, sg, gso, gro
2. **ethtool -K gro off** 后 rx_gro_packets 不增长
3. **ethtool -K gro on** 后 rx_gro_packets 增长
4. **ndo_set_features** 每次 ethtool -K 触发 feature_set_count++
5. **smoke test** 收发包正常
