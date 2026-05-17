# REVIEW_BUNDLE: lab-bpftrace-netdev-observe

## Metadata

- Date: 2026-05-11T22:33:36+08:00
- Kernel: 6.8.0-111-generic
- Record dir: /home/wq7/workspace/driver-lab/linux-driver-lab/track-ebpf-observability/lab-bpftrace-netdev-observe/records/20260511-222909-bpftrace-netdev-observe
- Interface hint: ens192
- Mode: tracepoint-first, kprobe optional, no BEGIN/END blocks

## Files

| File | Status |
|---|---|
| ENV_CHECK.txt | MISSING |
| PROBE_POINTS.txt | MISSING |
| XDP_CLEAN.txt | DONE |
| RX_TRACEPOINT.log | DONE |
| TX_TRACEPOINT.log | DONE |
| SOFTIRQ_TRACEPOINT.log | DONE |
| KPROBE_OPTIONAL.log | MISSING |
| COLLECT_STATS.txt | DONE |

## Judgement

| Item | Result |
|---|---|
| PASS_ENV | NO |
| PASS_PROBE_LIST | NO |
| XDP_CLEAN_OR_WARNED | ALREADY_OFF |
| PASS_TRACEPOINT_RX | YES |
| PASS_TRACEPOINT_TX | YES |
| PASS_SOFTIRQ | YES |
| KPROBE_OPTIONAL | NOT_RUN |

## Traffic Evidence

| Log | Non-zero evidence |
|---|---|
| RX_TRACEPOINT.log | YES |
| TX_TRACEPOINT.log | YES |
| SOFTIRQ_TRACEPOINT.log | YES |
| KPROBE_OPTIONAL.log | NO |

## Notes

- This lab now avoids bpftrace BEGIN/END blocks to bypass BEGIN_trigger/END_trigger compatibility issues.
- Tracepoints are the acceptance path because they are more stable across kernel builds than kprobes.
- kprobe failures caused by BTF/notrace/symbol differences should be recorded as NOTE, not as lab failure.
- If tracepoint logs exist but counters are zero, classify as PASS_TRACEPOINT_SMOKE but not PASS_TRAFFIC_OBSERVED.
- If ens192 has an existing XDP program, skb-level tracepoints may not see packets; detach it with 02_clean_xdp_if_attached.sh when safe.
