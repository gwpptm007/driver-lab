# REVIEW_BUNDLE - project-dpdk-media-gateway-lite

## Record directory

`/e/02_Learning/2026/gitcode/driver-lab/linux-driver-lab/track-dpdk/project-dpdk-media-gateway-lite/records/20260507-225409-media-gateway-lite`

## Test command

```bash
./scripts/05_run_vdev_null_pair_smoke.sh
```

EAL 参数：
```
-l 0-1 -n 4 --file-prefix media_gateway_lite_vdev_null --no-pci --vdev net_null0 --vdev net_null1
```

App 参数：
```
--run-seconds 20 --stats-period 2 --burst-size 32 --promisc 1 --udp-only 1
--swap-mac 1 --strict-rules 1
--rule0 0:1 --rule0-name access_to_core
--rule1 1:0 --rule1-name core_to_access
```

## Checklist

| Item | Status |
|---|---|
| BUILD.log | DONE |
| MEDIA_GATEWAY_VDEV_NULL_PAIR.log | DONE |
| COMPARE_STATS.txt | (手动分析) |

## Evidence grep

| Evidence | Found |
|---|---|
| media-gateway-lite starting | YES |
| port 0/1 started | YES |
| enter media gateway loop | YES |
| media-gateway-lite software stats | YES |
| rte_eth_stats | YES |
| bye | YES |

## Final stats (last iteration)

```
port 0: rx=1170561856 rx_bytes=74915958784 tx=0 drops=1170561856
  ipv4=0 udp=0 non_udp=1170561856 drop_non_udp=1170561856
port 1: rx=1170561856 rx_bytes=74915958784 tx=0 drops=1170561856
  ipv4=0 udp=0 non_udp=1170561856 drop_non_udp=1170561856
```

## Analysis

| 现象 | 原因 | 判定 |
|------|------|------|
| rx=1.17B | net_null 持续产生 dummy 包 | ✅ 正常 |
| ipv4/udp=0 | net_null 产生的是非 IP/UDP 包 | ✅ 预期 |
| drop_non_udp=1.17B | udp_only=1 策略过滤了所有非 UDP | ✅ 预期 |
| tx=0 | net_null 不转发；单端口也无转发目标 | ✅ 预期 |

## Verdict

**PASS_SMOKE** ✅

- EAL 初始化正常
- 2 端口发现和启动正常
- 规则配置正确加载（2 条规则）
- poll loop 运行正常
- udp_only 过滤策略生效
- 程序正常退出

**未达到 PASS_TRAFFIC 的原因**：net_null 产生的是 dummy 非 UDP 包，不是真实 UDP 流量。

## Next steps

1. 去掉 `udp-only` 或改用真实 UDP 发包器测试 PASS_TRAFFIC
2. 添加 KNI 支持，实现 DPDK+KNI 协同架构
3. 测试 rewrite 功能：`./scripts/06_run_rule_rewrite_demo.sh`
