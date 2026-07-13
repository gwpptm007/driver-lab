# project-dpdk-media-gateway-lite_report

## 目标

实现简化版 DPDK 用户态媒体网关。

## 当前状态

TESTED (2026-06-07, pcap PMD path).

## 测试结论

| 等级 | 状态 | 证据 |
|------|------|------|
| PASS_BUILD | ✅ | meson build 成功, 见 `records/20260607-pcap-traffic-test/BUILD.log` |
| PASS_SMOKE | ✅ | 程序启动, 端口发现, 进入 loop, 正常退出 |
| PASS_RULE_CONFIG | ✅ | rule 0 正确加载: dir=0:1, match_dst_port=9000, rewrite 字段完整 |
| PASS_PCAP_FUNCTIONAL | ✅ | rx=161830784, ipv4=161830784, udp=161830784 (port 0, infinite pcap replay) |
| PASS_PCAP_FORWARDING | ✅ | port 1 null PMD tx=161830784, rule 0 hit=161830784 |
| PASS_PCAP_REWRITE | ✅ | rewrite=161830784, rule 0 rewrite=161830784 |

这些计数证明软件功能路径和所有权闭环，不代表外部 wire 流量、真实 NIC 转发或线速性能。

详见:
- `records/20260607-pcap-traffic-test/TEST_COMMAND.md`
- `records/20260607-pcap-traffic-test/MEDIA_GATEWAY_PCAP_TRAFFIC_TEST.log`
