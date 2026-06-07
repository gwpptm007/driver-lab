# Cross-Trace Demo Report

Generated: 2026-06-07 17:00:30
Duration: 15s
Record dir: /home/wq7/workspace/driver-lab/linux-driver-lab/project-linux-network-data-plane/records/20260607_170014-cross-trace

## Topology

```text
                  ┌──────────────────┐
  gen_udp_pcap.py │ UDP pcap (500) │
                  └────────┬─────────┘
                           │ infinite replay
                           ▼
                  ┌──────────────────┐
    DPDK fastpath │ net_pcap0 (rx)   │ ← userspace PMD, bypass kernel
                  │ classify→forward │
                  │ net_null0 (tx)   │
                  └──────────────────┘
                           │
          ┌────────────────┼────────────────┐
          │ no kernel path │                │
          ▼                 ▼                ▼
   ┌──────────────┐ ┌──────────────┐ ┌──────────────┐
   │ napi_poll    │ │netif_receive │ │ dev_queue    │
   │ count: ???   │ │_skb count:?? │ │ _xmit: ???   │
   └──────────────┘ └──────────────┘ └──────────────┘
          ▲                 ▲                ▲
          │       bpftrace packet_watcher.bt       │
          └────────────────┼────────────────┘
```

## Results

### DPDK Userspace Fastpath

| Metric | Value |
|--------|-------|
| rx packets | `170006720` |
| ipv4 packets | `170006720` |
| tx packets | `170006720` |

```
port 0: rx=170006720 rx_bytes=13090517440 tx=170006720 tx_bytes=10880430080 tx_failed=0 arp=0 ipv4=170006720 udp=170006720 non_udp=0 rewrite=0 drop_short=0 drop_non_udp=0 drop_no_peer=0
```

### Kernel Path (bpftrace kprobes)

| Metric | Value |
|--------|-------|
| napi_poll calls | `0` |
| netif_receive_skb calls | `0` |
| dev_queue_xmit calls | `0` |

### Verdict

**PASS: DPDK fastpath processed real UDP traffic (rx=170006720).**

**PASS: Kernel NAPI/skb path was NOT triggered** — DPDK userspace PMD
completely bypassed the kernel network stack. This is the expected behavior
for a DPDK data plane.

## Raw Logs

- [fastpath_dpdk.log](fastpath_dpdk.log)
- [bpftrace_watcher.log](bpftrace_watcher.log)
