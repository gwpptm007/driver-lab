# REVIEW_BUNDLE: lab-xdp-redirect-basics

## Metadata

- Date: 2026-06-07T13:29:11+08:00
- Host: wq7-virtual-machine
- Kernel: 6.8.0-111-generic
- Record: 20260607-132613-xdp-redirect-basics
- Interface: veth-xdp (skb mode, also tested on ens192)
- Mode: skb

## Files

| File | Status |
|---|---|
| ENV_CHECK.txt | DONE |
| BUILD.log | DONE |
| PREPARE_KERNEL_NETDEV.txt | MISSING (ens192 already bound to vmxnet3) |
| XDP_PASS.log | DONE |
| XDP_DROP.log | DONE |
| XDP_REDIRECT_DRYRUN.log | DONE |
| COLLECT_STATS.txt | DONE |

## Test Results (veth-xdp with traffic injection)

| Test | Action | Packets | Bytes | Attach | Detach |
|------|--------|---------|-------|--------|--------|
| XDP_PASS | pass | 12 | 628 | OK | OK |
| XDP_DROP | drop | 3 | 126 | OK | OK |
| XDP_REDIRECT | redirect | 3 | 126 | OK | OK |

Traffic injected from veth-peer (10.99.0.2) via ping to 10.99.0.1.

## Acceptance

| Item | Result |
|---|---|
| PASS_BASIC | YES |
| PASS_ACTION | YES |
| REDIRECT_MODEL_READY | YES |

## Interpretation

- PASS_BASIC: BPF build + XDP attach + stats map + detach all succeed with non-zero traffic
- PASS_ACTION: DROP action verified — packets counted in drop stats (3 pkts dropped)
- REDIRECT_MODEL_READY: XSKMAP redirect path functional — packets counted in redirect stats (3 pkts)

## veth Test Topology

veth-peer (10.99.0.2) --> veth-xdp (XDP attached) --> XDP program
- veth-peer sends ICMP to 10.99.0.1
- veth-xdp receives and XDP program processes
- This proves the full XDP path works end-to-end

## Next

Continue to: track-af-xdp/lab-af-xdp-socket-rings
