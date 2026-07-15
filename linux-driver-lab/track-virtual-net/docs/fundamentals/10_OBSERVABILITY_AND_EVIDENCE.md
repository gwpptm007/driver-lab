# 10：观测与证据设计

## 先定义要证明的命题

每条记录应对应一个可证伪命题，而非“保存一堆命令输出”。例如：

| 命题 | 需要的组合证据 |
| --- | --- |
| TAP 已接入 bridge | `ip -d link` + `bridge link` |
| guest MAC 被 bridge 学习 | guest MAC 来源 + `bridge fdb` |
| guest 能与 host bridge IP 通信 | guest IP/route/neigh + ping + host bridge IP |
| guest A 到 B 是 L2 forwarding | 两个 TAP port、A/B MAC 的 FDB + 双向 ARP/ICMP 抓包 |
| vhost 模式是可用且被请求的 | QEMU 参数 + `/dev/vhost-net`/模块 + QEMU 输出 |

每种证据都有盲区；证据集的价值在互相约束。

## 四层观测面

### 1. 配置事实

收集 QEMU command line、host `ip -d link`、`bridge link/vlan/fdb`、guest `ip addr/route/link`。它解释“系统被怎样配置”。

### 2. 数据事实

用定时、带 MAC 的 `tcpdump` 在 TAP/bridge 上采集 ARP/ICMP 或固定 5-tuple 流量。它解释“在这个观察点出现过什么帧”。

### 3. 计数事实

收集 guest/host 接口统计、drop/error、softirq/CPU 和工具统计。它用于发现丢失、方向不对或负载不均；不要把不同粒度计数强行做一一对应。

### 4. 时间与环境事实

保存开始/结束时间、host/guest kernel、QEMU 版本、CPU/vCPU、offload、模块、权限和命令退出码。没有环境事实，后续无法复现或解释差异。

## 抓包规则

```bash
# 保留 L2 MAC、时间戳和足够的解码信息；实际接口名按实验替换。
sudo tcpdump -ni tap-vnet-a -e -vv '(arp or icmp)'
sudo tcpdump -ni br-vnet0 -e -vv '(arp or icmp)'
```

建议一次只抓必要接口，写明方向和过滤条件。抓包本身有开销，也可能因 offload/抓包点而显示不同帧形态；原始 pcap 与文本摘要都应保留。

## 推荐 records 结构

```text
records/<timestamp>-<experiment>/
  environment.md          # host/guest/QEMU/modules/capabilities
  topology.md             # interfaces, MAC/IP, bridge ports, diagram in text
  qemu-command.txt
  host-ip-link.txt
  bridge-link.txt
  bridge-fdb.txt
  guest-network.txt
  workload.txt
  capture-*.pcap
  stdout-stderr.txt
  summary.md              # conclusion, limits, cleanup status
```

不要在 records 中保存明文 sudo 密码、私钥、生产 IP 或不必要的 payload。

## 负结论也必须保存

例子：`/dev/vhost-net` 不存在、QEMU 报 vhost 不支持、multiqueue 未协商、guest 没有预期 driver。负结论应写明探测命令、原始输出、是否影响基础 Lab、下一步需要什么环境；它比一句“环境不支持”更可用。

## 报告模板

每个 Lab/Project 的结论使用四段：

1. **已验证**：哪个命题、在哪个环境、由哪些文件支持；
2. **未验证**：缺少什么能力或证据；
3. **不应推断**：当前数据不能推出哪些性能/实现结论；
4. **可复现步骤**：最小命令、输入和清理动作。

这使文档在后续接入 vhost-user、DPDK 或更多 guest 时仍可扩展，而不会失去证据边界。
