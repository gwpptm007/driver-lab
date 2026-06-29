# track-dpdk-advanced Roadmap

## Phase 1: lab-dpdk-mbuf-mempool-deep-dive

状态：`PASS_PCAP_METADATA`

交付：

- `dpdk-mbuf-inspect` C 程序。
- pcap PMD metadata 验证。
- mbuf/mempool 理解文档。
- 正式记录：`records/20260629-210538-mbuf-mempool/`

## Phase 2: lab-dpdk-rss-multiqueue

状态：`BLOCKED_PCAP_RSS`

交付：

- `dpdk-rss-queue-probe` C 程序。
- RSS capability / queue capability 探测。
- 证明 pcap PMD 当前 `max_rx_queues=1`、`reta_size=0`、`rss_offloads=0x0`。
- 正式记录：`records/20260629-211820-rss-multiqueue/`

## Phase 3: lab-dpdk-numa-burst-tuning

状态：`PASS_TUNING_METHOD`

交付：

- `dpdk-burst-cache-probe` C 程序。
- burst size 与 mempool cache size 矩阵。
- CPU/NUMA/pps 记录格式。
- 正式记录：`records/20260629-212218-numa-burst/`

## Phase 4: lab-dpdk-vfio-iommu-boundary

状态：`PASS_VFIO_IOMMU_BOUNDARY`

交付：

- UIO/VFIO/IOMMU 差异矩阵。
- vmxnet3 当前 driver/interrupt 取证。
- VFIO/IOMMU prerequisites checklist。
- 正式记录：`records/20260629-212638-vfio-iommu/`

## Phase 5: project-dpdk-l3-forwarder-lite

状态：`PASS_L3_FORWARDER_LITE`

交付：

- `dpdk-l3-forwarder-lite` C 程序。
- IPv4/UDP parse。
- ACL drop rule。
- route lookup。
- per-rule stats。
- pcap PMD traffic evidence。
- 正式记录：`records/20260629-213104-l3-forwarder/`

## Phase 6: project-dpdk-advanced-summary

状态：`PASS_ADVANCED_REPORT`

交付：

- final report。
- evidence index。
- tuning checklist。
- interview notes。
- resume material。
- RDMA transition notes。

