# 02_TEST_AND_VERIFY - 测试命令与执行记录

> 本文记录 Phase 5 在 `192.168.65.135` 测试机上的实际执行命令、日志位置、关键输出和验收判断。

## 1. 测试环境

```text
Host: 192.168.65.135
User: wq7
Remote repo: /home/wq7/workspace/driver-lab
Project: linux-driver-lab/track-dpdk-advanced/project-dpdk-l3-forwarder-lite
Record: records/20260629-213104-l3-forwarder/
```

环境日志：

```text
records/20260629-213104-l3-forwarder/ENV_CHECK.log
```

关键输出：

```text
DPDK version: 21.11.9
Python: 3.10.12
2MB hugepages: 1024
CPU lcores detected by EAL: 8
NUMA nodes detected by EAL: 1
```

## 2. 完整执行命令

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk-advanced/project-dpdk-l3-forwarder-lite
chmod +x scripts/*.sh tools/*.py
export RECORD_DIR="$PWD/records/$(date +%Y%m%d-%H%M%S)-l3-forwarder"
./scripts/00_check_env.sh
./scripts/01_build.sh
./scripts/02_run_pcap_l3_forward.sh
./scripts/03_collect_report.sh
cat "$RECORD_DIR/SUMMARY.md"
```

本次实际记录目录：

```text
records/20260629-213104-l3-forwarder/
```

## 3. Step 0: 环境检查

```bash
./scripts/00_check_env.sh
```

生成：

```text
ENV_CHECK.log
```

用途：

- 记录 kernel。
- 记录 DPDK 版本。
- 记录 Python 版本。
- 记录 hugepage 状态。

## 4. Step 1: 构建程序

```bash
./scripts/01_build.sh
```

生成：

```text
BUILD.log
```

关键输出：

```text
cc -O2 -g -Wall -Wextra ... main.c -o build/dpdk-l3-forwarder-lite ...
build/dpdk-l3-forwarder-lite: ELF 64-bit LSB pie executable
```

验收：

```text
PASS_BUILD
```

## 5. Step 2: 生成 pcap

脚本内部执行：

```bash
python3 tools/gen_l3_pcap.py "$PCAP_FILE" "$L3_PCAP_COUNT"
```

生成：

```text
l3_input.pcap
PCAP_GENERATE.log
```

关键输出：

```text
Generated 48 mixed IPv4/UDP packets -> .../l3_input.pcap
```

流量比例：

```text
12 packets: 10.20.0.77:9999 -> ACL drop
12 packets: 10.99.0.77:9000 -> route miss
24 packets: 10.20.0.77:9000 -> forward
```

## 6. Step 3: 运行 L3 forwarder

```bash
./scripts/02_run_pcap_l3_forward.sh
```

实际 DPDK 命令：

```bash
app/build/dpdk-l3-forwarder-lite \
  -l 0-1 -n 4 --no-pci \
  --file-prefix dpdk_l3_forwarder_lite_<timestamp> \
  --vdev "net_pcap0,rx_pcap=records/20260629-213104-l3-forwarder/l3_input.pcap" \
  --vdev "net_null1" \
  -- \
  --burst-size 16 \
  --mbuf-cache 250 \
  --max-idle-polls 100000
```

为什么用 `--no-pci`：

```text
本测试只验证软件数据面，避免扫描或操作真实网卡。
```

为什么用 `net_null1`：

```text
它作为 TX sink，能验证 tx_burst 成功，不需要真实链路对端。
```

## 7. Step 4: 运行结果

日志：

```text
L3_FORWARD.log
```

关键输出：

```text
CONFIG in_port=0 out_port=1 burst=16 nb_mbuf=8192 mbuf_cache=250
ROUTE[0] prefix=10.20.0.0/24 out_port=1
ACL[0] action=drop udp_dst_port=9999
RESULT rx_packets=48 rx_bytes=3264 forwarded_packets=24 forwarded_bytes=1608 acl_drops=12 route_miss_drops=12 non_ipv4_drops=0 parse_drops=0 tx_failed=0 polls=100003 empty_polls=100000
ROUTE_STATS[0] hits=24 bytes=1608
ACL_STATS[0] drops=12 bytes=816
```

解释：

| 字段 | 值 | 含义 |
|---|---:|---|
| `rx_packets` | 48 | pcap 中所有包都被 RX 到 |
| `forwarded_packets` | 24 | route hit 且 TX 成功 |
| `acl_drops` | 12 | 命中 UDP dst port 9999 |
| `route_miss_drops` | 12 | 不命中 `10.20.0.0/24` |
| `tx_failed` | 0 | net_null TX 没有失败 |
| `ROUTE_STATS[0].hits` | 24 | per-route stats 与 forward 对齐 |
| `ACL_STATS[0].drops` | 12 | per-ACL stats 与 drop 对齐 |

## 8. Step 5: 汇总验收

```bash
./scripts/03_collect_report.sh
cat records/20260629-213104-l3-forwarder/SUMMARY.md
```

验收结果：

```text
PASS_BUILD
PASS_ROUTE_CONFIG
PASS_L3_FORWARD
PASS_ACL_DROP
PASS_PER_RULE_STATS
PASS_PCAP_EVIDENCE
```

对应文件：

```text
records/20260629-213104-l3-forwarder/SUMMARY.md
```

## 9. 故障排查

### `need at least 2 DPDK ports`

检查 `--vdev` 是否同时包含：

```text
net_pcap0
net_null1
```

### `rx_packets=0`

检查：

- pcap 文件是否存在。
- `rx_pcap=` 路径是否正确。
- `tools/gen_l3_pcap.py` 是否成功执行。

### `forwarded_packets=0`

检查：

- route 是否打印：`ROUTE[0] prefix=10.20.0.0/24 out_port=1`
- pcap 中是否有目的 IP `10.20.0.77`
- ACL 是否把流量提前 drop。

### `tx_failed>0`

检查：

- `net_null1` 是否创建成功。
- TX queue 是否初始化成功。
- app 是否使用了正确的 `out_port=1`。

