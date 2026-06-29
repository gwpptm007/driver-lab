# L3 Forwarder Lite Summary

| Item | Result |
|------|--------|
| PASS_BUILD | PASS |
| PASS_ROUTE_CONFIG | PASS |
| PASS_L3_FORWARD | PASS |
| PASS_ACL_DROP | PASS |
| PASS_PER_RULE_STATS | PASS |
| PASS_PCAP_EVIDENCE | PASS |

## Raw stats

- RESULT rx_packets=48 rx_bytes=3264 forwarded_packets=24 forwarded_bytes=1608 acl_drops=12 route_miss_drops=12 non_ipv4_drops=0 parse_drops=0 tx_failed=0 polls=100003 empty_polls=100000
- ROUTE_STATS[0] hits=24 bytes=1608
- ACL_STATS[0] drops=12 bytes=816
- route_miss_drops=12
