# 02_TEST_AND_VERIFY

## 测试机环境

```text
Guest: Ubuntu 22.04.5 Desktop
Kernel: 6.8.0-110-generic
Hypervisor: VMware
管理口: ens33 / e1000 / 192.168.65.135   ← 脚本不得操作此口
DPDK口: ens192 / vmxnet3 / 0000:0b:00.0
DPDK driver: uio_pci_generic
```

## 测试路径

按推荐度排序：

| 路径 | 命令 | 依赖 | 说明 |
|------|------|------|------|
| pcap PMD | `./scripts/06_run_pcap_rx_test.sh` | 无 | **最稳，无需物理网卡/无需 sudo** |
| vdev null pair | `sudo ./scripts/05_run_vdev_null_pair_smoke.sh` | 无 | 验证程序启动和规则逻辑 |
| vmxnet3 单口 | `sudo ./scripts/03_run_single_port_smoke.sh` | vmxnet3 | RX/classify 验证 |
| 双口转发 | `sudo ./scripts/04_run_two_port_forwarding.sh` | 两个 DPDK 口 | forwarding 验证 |
| rewrite demo | `sudo ./scripts/06_run_rule_rewrite_demo.sh` | 两个 DPDK 口 | rewrite 验证 |

### pcap PMD 测试（推荐首选）

```bash
# 1. 生成 pcap
python3 tools/gen_udp_pcap.py /tmp/udp_test.pcap 500

# 2. 运行（--no-huge，无需 sudo）
./app/build/media-gateway-lite \
  -l 0-1 -n 4 --no-huge \
  --file-prefix mgw_pcap --no-pci \
  --vdev 'net_pcap0,rx_pcap=/tmp/udp_test.pcap,infinite_rx=1' \
  --vdev net_null0 \
  -- \
  --run-seconds 10 --stats-period 2 --burst-size 32 \
  --promisc 1 --udp-only 1 --swap-mac 0 --strict-rules 0 \
  --rule0 0:1 --rule0-dst-port 9000 \
  --rule0-rewrite-dst-ip 10.10.20.20 \
  --rule0-rewrite-dst-mac 52:54:00:00:00:02 \
  --rule0-rewrite-dst-port 10000

# 3. 解析统计
python3 tools/parse_gateway_stats.py <logfile>
```

拓扑：

```text
gen_udp_pcap.py (500 UDP pcap)
  → net_pcap0 rx (infinite replay)
  → media-gateway-lite (classify → rule match → rewrite → forward)
  → net_null0 tx (accept + discard)
```

## 验收标准

| 级别 | 条件 | 实测 (2026-06-07, pcap PMD) |
|------|------|------|
| PASS_SMOKE | 程序启动、端口发现、进入 loop | YES |
| PASS_TRAFFIC | rx>0, ipv4>0, udp>0 | rx=161830784, ipv4=161830784, udp=161830784 |
| PASS_FORWARDING | tx>0, rule_hit>0 | tx=161830784, rule_hit=161830784 |
| PASS_REWRITE | rewrite>0, rule_rewrite>0 | rewrite=161830784, rule_rewrite=161830784 |

## 故障排查

### rx 一直是 0

1. 没有外部发包源 → 先用 pcap PMD 隔离验证
2. 目的 MAC 不对 → 检查 pcap 中的 dst_mac
3. VMware 网络不通 → 检查 VMnet 配置
4. 网卡未绑定 DPDK driver → `dpdk-devbind.py --status`
5. 发包在同一个被 DPDK 接管的接口上执行 → 禁止

### tx=0

单口场景下 tx=0 正常（无转发目标）。要验证 forwarding 需要至少两个端口（物理/vdev/vhost）。

### rewrite=0

需要同时满足：IPv4/UDP 流量进入、规则方向匹配（in_port）、可选 match 条件满足、rewrite 字段已配置。
