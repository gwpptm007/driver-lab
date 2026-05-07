# 06_MIGRATION_CHECKLIST

## 从 DPDK v17 迁移到当前项目时的检查项

### 1. 环境与构建

```text
[ ] DPDK 版本确认
[ ] libdpdk.pc 是否存在
[ ] meson/ninja 或 Makefile 构建路径确认
[ ] hugepage 是否配置
[ ] DPDK 专用网卡是否和管理网卡分离
```

### 2. 设备绑定

```text
[ ] 确认管理口不绑定到 DPDK driver
[ ] 确认 DPDK 口 PCI BDF
[ ] 确认 vfio-pci 是否可用
[ ] 若 vfio 不可用，确认 uio_pci_generic 路线
[ ] 记录 dpdk-devbind.py --status
```

### 3. 数据面 API

```text
[ ] rte_eal_init
[ ] rte_pktmbuf_pool_create
[ ] rte_eth_dev_configure
[ ] rte_eth_rx_queue_setup
[ ] rte_eth_tx_queue_setup
[ ] rte_eth_dev_start
[ ] rte_eth_rx_burst
[ ] rte_eth_tx_burst
[ ] rte_eth_stats_get
```

### 4. 包解析与改写

```text
[ ] Ethernet header 长度检查
[ ] ether_type 判断
[ ] IPv4 ihl/total_length 检查
[ ] IPv4 protocol == UDP
[ ] UDP header 长度检查
[ ] MAC rewrite
[ ] IPv4 rewrite
[ ] UDP port rewrite
[ ] checksum 处理策略
```

### 5. mbuf 所有权

重点风险：

```text
tx_burst 成功后，mbuf 所有权交给 PMD，不能再访问该 mbuf。
```

推荐写法：

```c
uint32_t pkt_len = rte_pktmbuf_pkt_len(m);
uint16_t sent = rte_eth_tx_burst(port, queue, &m, 1);
if (sent == 1) {
    stats.tx++;
    stats.tx_bytes += pkt_len;
}
```

### 6. 统计和 records

```text
[ ] 软件 stats 按 port 打印
[ ] 软件 stats 按 rule 打印
[ ] rte_eth_stats 同步打印
[ ] parse 脚本按最后一次累计值判定，不重复累加
[ ] REVIEW_BUNDLE.md 包含命令、日志、结论
```

### 7. 验收等级

```text
PASS_BUILD:
  编译通过

PASS_SMOKE:
  EAL/port/loop/stats OK

PASS_TRAFFIC:
  rx/ipv4/udp 非 0

PASS_FORWARDING:
  tx/rule_hit 非 0

PASS_REWRITE:
  rewrite_hit 非 0，并有统计/抓包证明
```
