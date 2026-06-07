# eBPF Observability Evidence

## 对应章节

- `../docs/06_EBPF_OBSERVABILITY.md`

## 主入口

- `../../track-ebpf-observability/README.md`
- `../../track-ebpf-observability/project-linux-network-observability/START_HERE.md`

## 关键证据

- `../../track-ebpf-observability/project-linux-network-observability/src/net_observer.bpf.c`
- `../../track-ebpf-observability/project-linux-network-observability/src/net_observer.c`
- `../../track-ebpf-observability/project-linux-network-observability/src/net_observer.h`
- `../../track-ebpf-observability/project-linux-network-observability/scripts/01_build.sh`
- `../../track-ebpf-observability/project-linux-network-observability/scripts/02_run_observer.sh`
- `../../track-ebpf-observability/project-linux-network-observability/scripts/03_generate_report.sh`
- `../../track-ebpf-observability/project-linux-network-observability/reports/net-observe-20260606-193715.md`

## 已证明

```text
per-interface RX/GRO/TX/drop stats
drop reason table
per-CPU event distribution
path invariant analysis
Markdown report generation
```

## 示例观测能力

```text
RX -> GRO ratio
TX-QUEUE -> TX-XMIT consistency
DROP rate and reason
interface-level event aggregation
CPU-level distribution
```

## 边界

当前是实验型网络路径观测工具，不是生产级长期采集平台。
