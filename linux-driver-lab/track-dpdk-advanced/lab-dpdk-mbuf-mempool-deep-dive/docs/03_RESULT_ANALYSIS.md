# 测试结果分析

## 记录目录

```text
records/20260629-210538-mbuf-mempool
```

关键文件：

```text
ENV_CHECK.log
BUILD.log
PCAP_GENERATE.log
PCAP_METADATA.log
SUMMARY.md
udp_input.pcap
```

## Summary

```text
PASS_BUILD              PASS
PASS_PCAP_RX            PASS
PASS_MBUF_METADATA      PASS
PASS_MEMPOOL_CONFIG     PASS
PASS_STATS_CONSISTENCY  PASS
```

## 为什么只收到 32 个包

pcap 里生成了 64 个 UDP packet：

```text
Generated 64 UDP packets
```

程序输出：

```text
software_rx_packets=32
samples_printed=8
```

原因是当前程序在这个条件满足时提前退出：

```c
if (stats->rx_packets > 0 && stats->samples_printed >= cfg.sample_limit) {
    break;
}
```

`burst_size=32`，第一次 `rx_burst()` 收到 32 个包；其中前 8 个被打印；达到 sample limit 后退出。

这个行为符合 Phase 1 目标：观察 mbuf metadata，而不是 drain 完 pcap。

如果后续想验证完整 drain，可以新增参数：

```text
--drain-all 1
```

或删除 sample 满足后的提前退出。

## mbuf 字段解释

典型 sample：

```text
MBUF_SAMPLE index=0 port=0 mbuf_port=0 buf_addr=0x10080cc40 buf_iova=0x10080cc40 data_off=128 data_len=67 pkt_len=67 nb_segs=1 ol_flags=0x800000 packet_type=0x0 rss_hash=0x0 refcnt=1
```

解释：

| 字段 | 结果 | 说明 |
|------|------|------|
| `port` / `mbuf_port` | `0` | packet 来自 port 0 |
| `buf_addr` | 非空地址 | mbuf data buffer 虚拟地址 |
| `buf_iova` | 接近 `buf_addr` | 当前 EAL 使用 IOVA as VA |
| `data_off` | `128` | packet data 前有 headroom |
| `data_len` | `67` | 当前 segment 长度 |
| `pkt_len` | `67` | 整包长度，单段包等于 data_len |
| `nb_segs` | `1` | 小 UDP 包，非 scatter-gather |
| `ol_flags` | `0x800000` | PMD 填入的 offload metadata |
| `packet_type` | `0x0` | pcap PMD 未解析 packet type |
| `rss_hash` | `0x0` | Phase 1 未启用 RSS |
| `refcnt` | `1` | 当前 mbuf 单引用 |

## mempool 结果

```text
mempool_size=8192
mempool_cache_size=250
mempool_socket=0
mempool_avail_start=8192
mempool_in_use_start=0
mempool_avail_end=8192
mempool_in_use_end=0
```

结论：

- mempool 创建成功。
- RX 过程中 mbuf 被 PMD 分配给应用。
- 应用观察后调用 `rte_pktmbuf_free()`。
- 结束时可用对象数回到 8192，没有明显 mbuf 泄漏。

## stats 对齐

```text
software_rx_packets=32
software_rx_bytes=2144
ethdev_ipackets=32
ethdev_ibytes=2144
```

结论：

```text
PASS_STATS_CONSISTENCY
```

软件计数和 PMD 计数一致，说明 `rx_burst()` 收到的 packet 数量与 ethdev RX 统计对齐。

## 环境问题记录

### hugepages 初始为 0

首次运行失败：

```text
EAL: FATAL: Cannot get hugepage information.
```

修复：

```bash
echo 1024 | sudo tee /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
```

### /dev/hugepages 权限不足

第二次失败：

```text
EAL: get_seg_fd(): open '/dev/hugepages/dpdk_mbuf_inspectmap_0' failed: Permission denied
```

修复：

```bash
sudo chmod 1777 /dev/hugepages
```

这属于测试机环境准备问题，不是 app 逻辑问题。

## Phase 1 结论

Phase 1 已证明：

```text
pcap PMD RX path 可以稳定触发 rte_eth_rx_burst()
mbuf metadata 可以被打印和解释
mempool 配置和对象归还状态可观测
software stats 与 ethdev stats 可以对齐
```

下一步进入 Phase 2 时，重点从单队列 pcap PMD 转向：

```text
RSS capability query
multi-queue config
queue-to-core mapping
当前 VMware/vmxnet3 环境边界
```
