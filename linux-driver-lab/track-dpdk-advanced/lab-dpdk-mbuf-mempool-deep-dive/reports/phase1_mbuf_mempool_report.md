# Phase 1 Report: mbuf / mempool

## 目标

验证 `dpdk-mbuf-inspect` 能在 pcap PMD 路径下接收 UDP packet，并打印关键 `rte_mbuf` metadata、mempool 配置和 stats 对齐结果。

## 测试环境

```text
Host: 192.168.65.135
DPDK: 21.11.9
PMD: net_pcap
Record: records/20260629-210538-mbuf-mempool/
```

测试前确认 2MB hugepages 已配置：

```text
/sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages:1024
```

并允许普通用户创建 DPDK hugepage backing files：

```bash
sudo chmod 1777 /dev/hugepages
```

## 执行命令

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk-advanced/lab-dpdk-mbuf-mempool-deep-dive
export RECORD_DIR="$PWD/records/20260629-210538-mbuf-mempool"
./scripts/00_check_env.sh
./scripts/01_build.sh
./scripts/02_run_pcap_metadata.sh
./scripts/03_collect_report.sh "$RECORD_DIR"
cat "$RECORD_DIR/SUMMARY.md"
```

## 验收结果

```text
PASS_BUILD              PASS
PASS_PCAP_RX            PASS
PASS_MBUF_METADATA      PASS
PASS_MEMPOOL_CONFIG     PASS
PASS_STATS_CONSISTENCY  PASS
```

关键计数：

```text
software_rx_packets=32
samples_printed=8
```

## 关键观察

mbuf sample 中已经观察到：

```text
data_off=128
data_len=67
pkt_len=67
nb_segs=1
ol_flags=0x800000
rss_hash=0x0
refcnt=1
```

这些字段说明：

- packet data 从 mbuf buffer 的 `data_off=128` 处开始。
- 当前测试包是单 segment，`data_len == pkt_len`。
- Phase 1 未启用 RSS，因此 `rss_hash=0x0`。
- 每个 sample 处理后都会释放 mbuf，不产生 mempool 泄漏。

## 结论

Phase 1 达到 `PASS_PCAP_METADATA`。当前已经能用 pcap PMD 构造可复现流量，观察 mbuf metadata 和 mempool 配置，并完成软件 RX stats 与 ethdev RX stats 对齐。

更详细的理解和测试过程见：

```text
../docs/04_DEEP_LEARNING.md
../docs/02_TEST_AND_VERIFY.md
```

## 边界

本阶段只证明 pcap PMD RX path 下的 mbuf/mempool metadata 观察能力，不覆盖：

- 真实 NIC RX 性能。
- RSS 多队列。
- NUMA locality。
- burst size 性能影响。
- TX path。
- L3 forwarding。

