# DPDK Tuning Checklist

## Memory

- Confirm hugepages are configured before running DPDK.
- Record `nb_mbuf`, mempool cache size, and socket ID.
- Avoid comparing performance runs when mbuf count/cache changes silently.

## Burst

- Start with burst sizes `1, 4, 16, 32, 64`.
- Record RX packet count, byte count, empty polls, duration, and pps.
- Treat pcap PMD pps as methodology evidence, not NIC line-rate evidence.

## Queue / Core

- Record PMD capability before assuming RSS.
- Check `max_rx_queues`, `reta_size`, and `rss_offloads`.
- Keep queue-to-core mapping explicit in docs and commands.

## Driver Binding

- Identify management NIC before any bind/unbind.
- Do not bind the SSH NIC away from kernel driver.
- VFIO requires IOMMU groups and viable isolation.
- UIO is easier to use, but lacks VFIO-style IOMMU isolation.

## Evidence Discipline

- Keep build log, env log, run log, generated pcap, and summary together.
- Separate `PASS` from `BLOCKED` honestly.
- Never infer RX success from TX success.

