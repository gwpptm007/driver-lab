# records

这里存放 queue/poll observe 的真实实验记录。

## 测试结果 (2026-04-25)

### 测试环境
- QEMU guest with virtio-net-pci
- 测试目录: lab-virtio-net-queue-poll-observe
- 测试方式: ping window test

### 测试结果

**Ping Window Test (20 ping)**
- RX packets: 1 → 22 (delta +21)
- TX packets: 6 → 28 (delta +22)
- Ping 结果: 20 transmitted, 20 received, 0% loss, RTT avg 0.978ms

**Trace Chain 验证成功**
```
napi_poll: napi poll on napi struct for device eth0 work 0 budget 64
  → netif_receive_skb: dev=eth0 skbaddr=... len=50
```
- 共捕获 63 条 trace 事件
- 成功观察到 RX 事件推进链: queue event → napi_schedule → poll(budget) → netif_receive_skb

### 记录目录模板

建议每一轮都创建独立目录，例如：

```text
records/20260425_201500-ping-window/
```

目录内建议至少包含：

- `window_note.md`
- `SUMMARY.md`
- `CHAIN_REVIEW_NOTE.md`
- `IDLE_PING_IPERF_COMPARE.md`
- `idle_before_*`
- `idle_after_*`
- `ping_before_*`
- `ping_after_*`
- `iperf_before_*`
- `iperf_after_*`
- `*_ethtool_S.diff`
- `ping_output.txt`
- `iperf_output.txt`
- `trace.txt` (tracefs 输出)

### 测试流程

```bash
# 1. 启动 QEMU guest
# 2. 在 guest 内执行 bootstrap
./scripts/bootstrap_record_dir.sh

# 3. 执行 idle baseline
./scripts/run_idle_window.sh eth0 <record-dir>

# 4. 执行 ping workload
./scripts/run_ping_window.sh eth0 10.0.2.2 <record-dir>

# 5. 执行 iperf3
./scripts/run_iperf_window.sh eth0 10.0.2.2 <record-dir>

# 6. 汇总分析
./scripts/summarize_deltas.sh <record-dir>
```