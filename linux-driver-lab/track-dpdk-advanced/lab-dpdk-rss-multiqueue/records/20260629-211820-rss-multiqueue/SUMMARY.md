# RSS / multiqueue Phase 2 Summary

| Item | Result |
|------|--------|
| PASS_BUILD | PASS |
| QUEUE_CONFIG | BLOCKED_QUEUE_CONFIG |
| RSS_QUERY | BLOCKED_RSS |
| QUEUE_TO_CORE_DOC | PASS_QUEUE_TO_CORE_DOC |

## Capability

- driver_name=net_pcap
- max_rx_queues=1
- reta_size=0
- rss_offloads=0x0

## Queue map

queue_map rxq=0 lcore=1
queue_map rxq=1 lcore=2

## Blocked reasons

blocked_reason=no_rss_offloads_or_reta
blocked_reason=max_rx_queues_lt_requested requested=2 max_rx_queues=1
