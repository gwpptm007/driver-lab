# AF_XDP mini forwarder review bundle

## Record
`/home/wq7/workspace/driver-lab/linux-driver-lab/track-af-xdp/project-af-xdp-mini-forwarder/records/20260607-140717-af-xdp-mini-forwarder`

## Files
| File | Status |
|---|---|
| ENV_CHECK.txt | MISSING |
| BUILD.log | DONE |
| PREPARE_KERNEL_NETDEV.txt | MISSING |
| FORWARDER_DROP.log | DONE |
| FORWARDER_REFLECT.log | DONE |
| COLLECT_STATS.txt | DONE |
| TRAFFIC_HINT.txt | MISSING |

## Acceptance
| Item | Result |
|---|---|
| PASS_BUILD | YES |
| PASS_DROP_SMOKE | YES |
| PASS_DROP_FINAL | YES |
| PASS_REFLECT_SMOKE | YES |
| PASS_REFLECT_FINAL | YES |

## Parsed stats
```text
== parsed forwarder stats ==
FORWARDER_DROP.log: rx=3 rx_bytes=126 tx=0 tx_bytes=0 drop=3 fill=3 tx_full=0 comp=0 empty=83
FORWARDER_REFLECT.log: rx=3 rx_bytes=126 tx=3 tx_bytes=126 drop=0 fill=3 tx_full=0 comp=3 empty=92
SUMMARY rx=6 tx=3 comp=3
PASS_TRAFFIC=YES
PASS_TX_REFLECT=YES
```

## Notes
- 无流量时 rx/tx 为 0 是正常的 smoke 结果，不代表失败。
- PASS_TRAFFIC 需要 rx_packets > 0，需要外部发包。
- PASS_TX_REFLECT 需要 tx_packets > 0 和 comp_packets > 0。
