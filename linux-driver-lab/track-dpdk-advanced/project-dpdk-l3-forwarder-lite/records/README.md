# records

Runtime evidence for `project-dpdk-l3-forwarder-lite`.

## Official run

```text
20260629-213104-l3-forwarder/
```

Files:

- `ENV_CHECK.log`: DPDK version, Python version, hugepage state.
- `BUILD.log`: build output for `dpdk-l3-forwarder-lite`.
- `PCAP_GENERATE.log`: generated mixed forward/drop/miss pcap.
- `L3_FORWARD.log`: runtime output and stats.
- `SUMMARY.md`: acceptance matrix.
- `l3_input.pcap`: reproducible input traffic.
