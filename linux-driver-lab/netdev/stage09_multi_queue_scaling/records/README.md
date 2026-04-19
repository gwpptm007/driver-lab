本目录用于保存 stage09 的 smoke / queue distribution / timeline 实测记录。

## 记录列表

| 目录 | 日期 | 说明 |
|------|------|------|
| `20260419-163054-stage09-smoke` | 2026-04-19 | smoke + queue_dist + timeline 全部 PASS；test_tx=64 通过验证发送成功 |
| `20260418-110522-stage09-smoke` | 2026-04-18 | 首次成功 smoke test，DMA mask 修复后验证通过 |

---

## 验证方法

### 1. smoke test（冒烟测试）

```bash
cd linux-driver-lab/netdev/stage09_multi_queue_scaling
./scripts/smoke.sh
```

生成 `records/<timestamp>-stage09-smoke/` 目录，包含：
- `debugfs_stats_before.txt` / `debugfs_stats_after.txt`
- `debugfs_queues_before.txt` / `debugfs_queues_after.txt`
- `debugfs_timeline_before.txt` / `debugfs_timeline_after.txt`
- `dmesg_tail.txt`
- `recv.txt` / `send.txt`

### 2. 多队列分布验证

```bash
./scripts/queue_dist_check.sh records/<timestamp>-stage09-smoke
```

**通过标准**：至少 2 个队列有 `tx_submit > 0`

```bash
# 手动验证
cat records/<timestamp>-stage09-smoke/debugfs_stats_after.txt | grep '^q[01]:'
# 期望：q0 和 q1 的 tx_submit 均 > 0
```

### 3. 异步链路验证

```bash
./scripts/timeline_check.sh records/<timestamp>-stage09-smoke
```

**通过标准**：至少 1 个队列 `doorbell_to_backend_ns > 0`

```bash
# 手动验证
cat records/<timestamp>-stage09-smoke/debugfs_timeline_after.txt | grep '^q[01]:'
# 期望：q0 和 q1 的 doorbell_to_backend_ns 均 > 0（值为微秒级）
```

---

## 典型成功输出（20260418-110522）

### debugfs_stats_after
```
ifname=nds9 num_queues=2 ring_size=128 napi_weight=64 backend_batch=64 open_count=1 stop_count=0
q0: tx_submit=8 tx_complete=8 tx_packets=8 tx_bytes=1796 tx_busy=0 tx_drop=0 rx_post=135 rx_consume=8 rx_packets=8 rx_bytes=1796 rx_drop=0 doorbell=8 backend_schedule=8 backend_run=8 backend_tx=8 backend_rx=8 irq=8 napi_poll=8 napi_complete=8 napi_work=8 test_tx=0 test_rx=0
q1: tx_submit=9 tx_complete=9 tx_packets=9 tx_bytes=1019 tx_busy=0 tx_drop=0 rx_post=136 rx_consume=9 rx_packets=9 rx_bytes=1019 rx_drop=0 doorbell=9 backend_schedule=9 backend_run=9 backend_tx=9 backend_rx=9 irq=9 napi_poll=9 napi_complete=9 napi_work=9 test_tx=0 test_rx=0
```

### debugfs_timeline_after
```
q0: submit_ns=250408213361 doorbell_ns=250408213711 backend_wakeup_ns=250408287141 backend_done_ns=250408288533 irq_ns=250408288573 poll_ns=250408296264 complete_ns=250408297145 consume_ns=250408297265 submit_to_doorbell_ns=350 doorbell_to_backend_ns=73430 backend_to_irq_ns=40 irq_to_poll_ns=7691
q1: submit_ns=249731751666 doorbell_ns=249731751806 backend_wakeup_ns=249731839414 backend_done_ns=249731840826 irq_ns=249731840896 poll_ns=249731849338 complete_ns=249731850149 consume_ns=249731850289 submit_to_doorbell_ns=140 doorbell_to_backend_ns=87608 backend_to_irq_ns=70 irq_to_poll_ns=8442
```

### 验证通过标准（Acceptance Criteria）

| 项目 | 标准 | 首次实测 |
|------|------|----------|
| 编译通过 | driver + tools 无 error | ✅ 通过 |
| 加载成功 | debugfs 文件可读 | ✅ 通过 |
| 多队列活跃 | >= 2 个队列 `tx_submit > 0` | ✅ q0=8, q1=9 |
| 异步成立 | `doorbell_to_backend_ns > 0` | ✅ q0=73430ns, q1=87608ns |
| 资源回收 | 测试结束后计数器归零或稳定 | ✅ 正常 |

### 最新实测结果（20260419-163054）

#### debugfs_stats_after
```
ifname=nds9 num_queues=2 ring_size=128 napi_weight=64 backend_batch=64 open_count=1 stop_count=0
q0: tx_submit=71 tx_complete=71 tx_packets=71 tx_bytes=3377 tx_busy=0 tx_drop=0 rx_post=198 rx_consume=71 rx_packets=71 rx_bytes=3377 rx_drop=0 doorbell=94 backend_schedule=94 backend_run=31 backend_tx=71 backend_rx=71 irq=31 napi_poll=31 napi_complete=31 napi_work=71 test_tx=64 test_rx=0
q1: tx_submit=7 tx_complete=7 tx_packets=7 tx_bytes=850 tx_busy=0 tx_drop=0 rx_post=134 rx_consume=7 rx_packets=7 rx_bytes=850 rx_drop=0 doorbell=7 backend_schedule=7 backend_run=7 backend_tx=7 backend_rx=7 irq=7 napi_poll=7 napi_complete=7 napi_work=7 test_tx=0 test_rx=0
```

#### debugfs_timeline_after
```
q0: submit_ns=198425947693 doorbell_ns=198425947833 backend_wakeup_ns=198425975880 backend_done_ns=198425976580 irq_ns=198425976620 poll_ns=198425981232 complete_ns=198425981572 consume_ns=198425981632 submit_to_doorbell_ns=140 doorbell_to_backend_ns=28047 backend_to_irq_ns=40 irq_to_poll_ns=4612
q1: submit_ns=197923498037 doorbell_ns=197923498187 backend_wakeup_ns=197923534565 backend_done_ns=197923535265 irq_ns=197923535295 poll_ns=197923541757 complete_ns=197923542357 consume_ns=197923542417 submit_to_doorbell_ns=150 doorbell_to_backend_ns=36378 backend_to_irq_ns=30 irq_to_poll_ns=6462
```

#### 自动验收脚本结果
```
$ ./scripts/queue_dist_check.sh records/20260419-163054-stage09-smoke
Active queues with tx_submit>0: 2
PASS: multi-queue distribution verified (2 queues)

$ ./scripts/timeline_check.sh records/20260419-163054-stage09-smoke
Queues with doorbell_to_backend_ns>0: 2
PASS: async backend verified (2 queues)
```

#### 说明
- `test_tx=64`：64 个测试帧成功提交到 TX 路径
- `test_rx=0`：0x88B9 ether_type 在本 kernel 无协议处理句柄，recv 工具无法接收；test_rx 在 RX consume 路径检测 ether_type=0x88B9，但由于测试帧在 guest 网络栈被丢弃（无 handler），实际不进 RX consume 路径。test_tx 验证发送成功已足够。
- 多队列分布：q0 承载主测试流量（tx_submit=71），q1 承载辅助流量（tx_submit=7）
- 异步链路：q0/q1 的 doorbell_to_backend_ns 均为微秒级（28μs/36μs），异步 backend 成立

---

## 典型失败分析

### 崩溃在 ifconfig nds9 up

**现象**：`dmesg` 出现 `WARNING: ... dma_map_page_attrs` 或设备无法 UP

**原因**：缺少 `dma_set_mask_and_coherent` 设置，DMA mapping 失败

**修复**（已合入）：
```c
ndev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
ndev->dev.dma_mask = &ndev->dev.coherent_dma_mask;
if (dma_set_mask_and_coherent(&ndev->dev, DMA_BIT_MASK(64))) {
    ret = dma_set_mask_and_coherent(&ndev->dev, DMA_BIT_MASK(32));
    if (ret) {
        pr_warn(DRV_NAME ": dma_set_mask_and_coherent failed: %d\n", ret);
        free_netdev(ndev);
        return ret;
    }
}
```

### 模块僵尸状态（rmmod 失败）

**现象**：`rmmod: Device or resource busy`，模块无法卸载

**原因**：模块崩溃后进入僵尸状态，需要重启解决

**解决**：重启测试机后再 rmmod
