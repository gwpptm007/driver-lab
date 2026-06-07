# 02_ACCEPTANCE — 测试过程篇

## 1. 测试概览

| 项目 | 值 |
|------|-----|
| **测试日期** | 2026-06-06 19:36-19:37 CST |
| **测试机** | wq7-virtual-machine (VMware Workstation) |
| **内核版本** | 6.8.0-111-generic (Ubuntu 24.04) |
| **libbpf** | libbpf 1.x (测试机) / libbpf 0.5 (开发机兼容) |
| **clang** | clang 14 |
| **网卡** | ens33 (e1000), ens34 (e1000) |
| **流量** | ping -c 30 -i 0.2 8.8.8.8 -I ens33 |
| **最终判定** | **PASS_PROJECT_NET_OBSERVABILITY** |

## 2. 编译

```bash
cd project-linux-network-observability
make clean && make
```

结果: `Build OK -> build/net_observer`，0 error，1 strncpy warning (不影响功能)。
.BTF 和 .BTF.ext 存在，无 .rodata.str* section。

## 3. 运行测试

### 3.1 控制台运行

```bash
sudo build/net_observer -v -d 12
```

5 个 tracepoint 全部 attach。实时事件包含:
- RX/GRO/TX-QUEUE/TX-XMIT 正常流
- DROP 事件显示 `reason=NOT_SPECIFIED (2)`
- 多网卡 (ens33, ens34, lo) 均有事件

### 3.2 报告生成

```bash
sudo EBPF_DURATION=15 bash scripts/03_generate_report.sh
```

**per-interface 统计** (15s 运行):

| Interface | RX | TX-QUEUE | TX-XMIT | GRO | DROP |
|-----------|-----|----------|---------|-----|------|
| ens33 | 39 | 35 | 35 | 39 | 0 |
| ens34 | 4 | 16 | 16 | 0 | 0 |
| lo | 20 | 20 | 20 | 0 | 0 |
| <?> (DROP) | 0 | 0 | 0 | 0 | 2 |

**per-CPU 分布**: CPU3 集中 RX/GRO (ens33 IRQ), CPU0/2/7 分担 TX

**路径分析**:
- TX-QUEUE = TX-XMIT: 71 = 71 (100%) -> OK
- DROP rate: 2/134 = 1.49%

## 4. 数据验证

| 不变量 | 预期 | 实际 | 判定 |
|--------|------|------|------|
| ens33: RX = GRO | 39 = 39 | 39 = 39 | PASS |
| lo: GRO = 0 | loopback 不走 NAPI | 0 | PASS |
| TX-QUEUE = TX-XMIT | 71 = 71 | 71 = 71 | PASS |
| DROP reason 非空 | NOT_SPECIFIED | reason=2 | PASS |
| DROP ifname 为空 | kfree_skb 无 name | `<?>` | PASS |
| Markdown 报告生成 | 文件存在 | reports/net-observe-*.md | PASS |

## 5. 包长度验证

| len | 协议 | 解释 |
|-----|------|------|
| 46 | ARP | 28 + 14 L2 + 4 |
| 84 | ICMP reply | 70 + 14 L2 |
| 98 | ICMP request | 84 + 14 L2 |
| 4186 | TCP segment | 大包 |

## 6. 可复现命令

```bash
# 编译
ssh wq7@192.168.65.135 "cd .../project-linux-network-observability && make"

# 运行+报告
ssh wq7@192.168.65.135 "
    echo 'wq123456!' | sudo -S bash -c '
        cd .../project-linux-network-observability &&
        ping -c 30 -i 0.2 8.8.8.8 -I ens33 > /dev/null 2>&1 &
        EBPF_DURATION=15 bash scripts/03_generate_report.sh
    '
"

# 拉取结果
scp -r wq7@192.168.65.135:.../reports/ ./reports/
scp -r wq7@192.168.65.135:.../records/ ./records/
```

## 7. 最终判定

| 检查项 | 状态 |
|--------|------|
| 5 tracepoint attach | PASS |
| ringbuf 事件消费 | PASS |
| per-interface 聚合 (ens33/lo/ens34) | PASS |
| per-CPU 分布表 | PASS |
| DROP reason 提取 (NOT_SPECIFIED=2) | PASS |
| 路径分析 (TX-QUEUE=TX-XMIT 100%) | PASS |
| Markdown 报告生成 | PASS |
| 03_generate_report.sh | PASS |
| records + reports 保存 | PASS |

**最终判定: PASS_PROJECT_NET_OBSERVABILITY**
