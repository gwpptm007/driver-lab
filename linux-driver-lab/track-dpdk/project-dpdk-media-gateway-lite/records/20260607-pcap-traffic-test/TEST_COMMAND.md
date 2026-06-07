# pcap PMD 真实 UDP 流量 + rewrite 测试

测试时间: 2026-06-07
测试机器: 192.168.65.135 (Ubuntu 22.04, DPDK 21.11.9)

## 测试拓扑

```
gen_udp_pcap.py (500 UDP packets)
    ↓
net_pcap0 (rx from pcap, infinite replay)
    ↓
media-gateway-lite (classify → rule match → rewrite → forward)
    ↓
net_null0 (tx accept + discard)
```

## 前置步骤

```bash
# 生成 pcap 文件
python3 tools/gen_udp_pcap.py /tmp/udp_test.pcap 500

# 编译 (含 TX mbuf 修复)
./scripts/01_build_app.sh
```

## 测试命令 (PASS_TRAFFIC + PASS_FORWARDING + PASS_REWRITE)

```bash
./app/build/media-gateway-lite \
  -l 0-1 -n 4 --no-huge \
  --file-prefix mgw_record \
  --no-pci \
  --vdev 'net_pcap0,rx_pcap=/tmp/udp_test.pcap,infinite_rx=1' \
  --vdev net_null0 \
  -- \
  --run-seconds 10 --stats-period 2 --burst-size 32 \
  --promisc 1 --udp-only 1 --swap-mac 0 --strict-rules 0 \
  --rule0 0:1 --rule0-dst-port 9000 \
  --rule0-rewrite-dst-ip 10.10.20.20 \
  --rule0-rewrite-dst-mac 52:54:00:00:00:02 \
  --rule0-rewrite-dst-port 10000
```

## 统计解析

```bash
python3 tools/parse_gateway_stats.py records/pcap-traffic-test/gateway.log
```

## 结果

```
samples=10
rx=323661568          rx_bytes=22818140544
tx=161830784           tx_bytes=12460970368
ipv4=323661568         udp=323661568
rewrite=161830784
rule_hit=161830784     rule_rewrite=161830784

verdict_hints:
PASS_SMOKE=YES
PASS_TRAFFIC=YES
PASS_FORWARDING=YES
PASS_REWRITE=YES
```

## 说明

- 使用 `--no-huge` 绕过 hugepage 要求 (vdev 无需 hugepage)
- net_null 默认 `copy=1` 导致 port 1 出现 rx（TX 包回灌到 RX 路径），
  port 1 的 drop_no_route 计数器可解释这些额外 rx 的丢弃
- pcap PMD 的 `infinite_rx=1` 确保测试期间持续有流量
