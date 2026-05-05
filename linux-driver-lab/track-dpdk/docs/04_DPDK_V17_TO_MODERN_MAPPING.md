# 04_DPDK_V17_TO_MODERN_MAPPING

## 用途

本文档帮助有**运营商/媒体面 DPDK v17**项目经验的工程师，快速理解当前 track 内容与旧经验的对应关系，并标注差异点。

---

## 旧 DPDK v17 经验 → 当前 track 映射

| 旧项目能力 | 当前 track 对应 | 说明 |
|---|---|---|
| UDP 收发 | `lab-dpdk-l2-forwarding` | `rte_eth_rx_burst` / `rte_eth_tx_burst` 基础不变 |
| ARP/IP/UDP 重写 | `project-user-space-fastpath` | `handle_ipv4_udp()` 中实现 MAC/IPv4/UDP rewrite |
| KNI 回内核 | 后续可和 tap/AF_XDP 对照 | 当前 track 专注纯用户态 |
| 按网元转发 | flow table / route table 扩展 | `project` 暂未涉及，后续可扩展 |
| 媒体面收包 | `rte_eth_rx_burst` | PMD 驱动不变，API 基本兼容 |
| 多网元转发 | multi-port / multi-queue 扩展 | 当前测试机仅 1 个 vmxnet3，限制了多口验证 |

---

## DPDK 版本演进差异

### DPDK v17 (旧)

```text
- 使用 Makefile 构建
- `rte_lcore_id()` / `RTE_LCORE_FOREACH`
- `rte_kni` 用于用户态和内核交换
- `rte_ring` / `rte_mempool` 基本形态
- 多进程模型：primary / secondary
```

### DPDK 21.11+ (当前 track 使用)

```text
- 使用 meson + ninja 构建  ← 重大变化
- `rte_eal_init()` 参数基本兼容
- KNI 已废弃，推荐 tap / AF_XDP
- `rte_flow` API 成熟，支持 flow classified
- 安全增强：Cryptodev / IPsec 更好集成
```

### 构建工具变化（需特别注意）

```bash
# 旧: make
cd app && make

# 新: meson + ninja (DPDK 21.11+)
cd app
meson setup build      # 配置
ninja -C build        # 编译
```

当前测试机 DPDK 21.11.9 使用 meson+ninja。

---

## 媒体面思维对照

### 旧 v17 媒体面思维

```
收到 packet → 解析 Header → Classification → 修改 Header → 发送
     ↑                                                          ↓
     ←─────────────── Ring / Queue ←──────────────────────────
```

### 当前 fastpath-lite 实现

```c
// RX: rte_eth_rx_burst
nb_rx = rte_eth_rx_burst(port, 0, pkts, burst_size);

// 分类: classify_and_rewrite
classify_and_rewrite(src_portid, mbuf)
    ├─ ARP: swap_mac → 转发
    ├─ IPv4/UDP: handle_ipv4_udp() → rewrite → 转发
    └─ 其他: udp_only? drop : swap_mac → 转发

// TX: rte_eth_tx_burst
rte_eth_tx_burst(dst_port, 0, pkts, nb_tx);
```

### 关键差异

| 方面 | v17 | 当前 |
|------|-----|------|
| mbuf API | 基本相同 | 更好 |
| checksum offload | 手动计算 | 可选，rewrite 时自动处理 |
| flow director | 常见 | 推荐用 rte_flow |
| 统计 | 自定义 | `rte_eth_stats_get` + 自定义 sw_stats |

---

## 下一步扩展方向

如果想把当前 fastpath-lite 发展为更贴近 v17 网元经验：

```
当前: fastpath-lite (单端口 / 简单 rewrite)
  ↓
目标: user-space fastpath (多核 / 多口 / flow table)
  ↓
扩展:
  ├─ multi-lcore (rte_lcore 分发)
  ├─ multi-port (vhost-user + 物理口)
  ├─ flow table (按 5-tuple 分类)
  ├─ control-plane (gRPC / socket API)
  └─ records/replay (流量回放)
```

---

## 快速对照表

| v17 关键词 | 当前等价 |
|---|---|
| `mbuf pool create` | `rte_pktmbuf_pool_create` |
| `rx_burst` | `rte_eth_rx_burst` |
| `tx_burst` | `rte_eth_tx_burst` |
| `port start` | `rte_eth_dev_start` |
| `device configure` | `rte_eth_dev_configure` |
| `queue setup` | `rte_eth_rx/tx_queue_setup` |
| `KNI` | tap / AF_XDP (不在当前 track 范围) |
| `Makefile` | meson + ninja |