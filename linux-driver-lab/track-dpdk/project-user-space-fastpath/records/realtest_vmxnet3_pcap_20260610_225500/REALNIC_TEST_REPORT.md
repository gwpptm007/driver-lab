# DPDK Real NIC Test Report — vmxnet3 PMD

**Date**: 2026-06-10
**Machine**: wq7-virtual-machine (VMware, Ubuntu 22.04)
**NIC**: ens192 (vmxnet3, 0000:0b:00.0, MAC: 00:0C:29:F8:F6:82)

## Test Setup

```text
  pcap PMD (port 1)  ──generate──>  fastpath-lite classify/forward  ──TX──>  vmxnet3 PMD (port 0)
  (infinite replay)                                                         (real NIC)
```

- Port 0: vmxnet3 (real NIC, driver=net_vmxnet3)
- Port 1: pcap PMD (traffic generator, 2000 UDP packets infinite replay)
- Run time: 8 seconds, burst=32, promisc=1

## Command

```bash
fastpath-lite -l 0-1 -n 4 --file-prefix fp_nic_test   -a 0000:0b:00.0   --vdev "net_pcap0,rx_pcap=/tmp/test_nic.pcap,infinite_rx=1"   -- --run-seconds 8 --stats-period 2 --burst-size 32 --promisc 1
```

## Results

### Software Stats

| Port | rx | tx | ipv4 | udp |
|------|----|----|------|-----|
| port 0 (vmxnet3) | 0 | 5,538,176 | 0 | 0 |
| port 1 (pcap) | 5,538,176 | 0 | 5,538,176 | 5,538,176 |

### Ethdev Stats

| Port | ipackets | opackets |
|------|----------|----------|
| port 0 (vmxnet3) | 0 | 5,538,176 |
| port 1 (pcap) | 5,538,176 | 0 |

### Key Metrics

- Packets forwarded through vmxnet3 NIC: **5,538,176**
- Bytes forwarded through vmxnet3 NIC: **426,439,552**
- Throughput: ~692,272 pps (5.5M / 8s)
- SW stats == ethdev stats: PASS
- vmxnet3 PMD driver: net_vmxnet3 (initialized OK)
- vmxnet3 MAC: 00:0C:29:F8:F6:82

## Verdicts

| Test | Status | Notes |
|------|--------|-------|
| PASS_INIT | DPDK EAL vmxnet3 init OK | driver=net_vmxnet3, rx_desc=1024, tx_desc=1024 |
| PASS_TX | 5.5M+ packets TX via vmxnet3 | Forwarded from pcap PMD through real NIC |
| PASS_STATS_CONSISTENCY | SW stats == ethdev stats | ipackets/opackets match rx/tx |
| PASS_CLASSIFY | IPv4/UDP classification OK | ipv4=5.5M, udp=5.5M on port 1 |
| BLOCKED_RX | Cannot verify RX path | VMware + UIO lacks MSI-X interrupt support for vmxnet3 RX |
| BLOCKED_E1000 | e1000 PMD incompatible | VMware virtual 82545EM cannot bind to UIO/VFIO |

## Topology Issue

```text
  ┌──────────────────────────────────────────────────┐
  │ VMware Host (Windows)                            │
  │  ├─ VMnet8 (NAT, 192.168.65.x)  ← Windows can send here
  │  ├─ VMnet? (192.168.100.x)      ← ens192 lives here
  │  │    No Windows host adapter                     │
  │  │    No other VM on this VMnet                   │
  │  └─ VMnet? (unknown)            ← ens34 lives here
  │                                                  │
  │  VMnet isolation: L2 frames cannot cross VMnets   │
  │  VMware NAT only forwards ICMP (ping), not UDP    │
  └──────────────────────────────────────────────────┘
```

## Root Cause: RX=0

RX=0 的根本原因是 **UIO 不提供 MSI-X 中断支持**，而 vmxnet3 PMD 的 RX 数据
路径依赖 MSI-X 中断通知新包到达。TX 不需要中断（主动写 MMIO 寄存器）所以正常。

VFIO-PCI 可以提供完整中断虚拟化，但 VMware guest 没有 IOMMU group，
VFIO 绑定失败。结论: VMware + UIO 环境无法走通 vmxnet3 RX。

## Conclusion

The vmxnet3 DPDK PMD TX path is functional. The TX path has been verified with
5.5M+ packets forwarded through the real NIC. The RX path cannot be verified
in the current VMware + UIO environment due to missing MSI-X interrupt support.

RX verification is intentionally deferred for this project stage. The code and
documentation should describe this as PASS_TX plus BLOCKED_RX, not as full
real-NIC bidirectional forwarding.

详见 [METHODOLOGY.md](METHODOLOGY.md) — 完整方法、命令、环境记录、根因分析。
