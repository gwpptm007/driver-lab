# stage03_napi_poll smoke test record

## 测试环境
- 测试机器: 192.168.65.135 (wq7@Ubuntu 22.04)
- 内核版本: 6.8.0-106-generic
- 测试日期: 2026-04-12
- 代码路径: /home/wq7/workspace/driver-lab/netdev/stage03_napi_poll/

## API 修复

### netif_napi_add API 变化 (Linux 6.8)
- **问题**: `netif_napi_add(dev, napi, poll, weight)` 在 Linux 6.8 减少为3个参数
- **修复**: 改用 `netif_napi_add_weight(dev, napi, poll, weight)`
- **本地已同步**: driver/netdev_stage03.c 已更新

## 测试步骤与结果

### 1. 构建
```bash
# 用户态工具（成功）
cd tools && gcc -o send_stage03_frame send_stage03_frame.c
                   -o recv_stage03_frame recv_stage03_frame.c

# 内核模块（成功，需 gcc-12 + netif_napi_add_weight 修复）
make -C /lib/modules/6.8.0-106-generic/build M=$(pwd) modules CC=gcc-12
# → driver/netdev_stage03.ko 编译成功
```

### 2. NAPI 模式测试（rx_mode=napi, loop_mode=copy）

```bash
sudo insmod driver/netdev_stage03.ko rx_mode=napi loop_mode=copy
```
**dmesg 输出：**
```
netdev_stage03: registered ifname=nds3 rx_mode=napi loop_mode=copy napi_weight=8 max_queue_depth=1024
```

**debugfs 统计（NAPI 模式）：**
```
rx_mode=napi
loop_mode=copy
napi_weight=8
tx_packets=45
rx_packets=45
copy_built=45
clone_built=0
direct_inject_count=0
napi_inject_count=45
netif_receive_success=45
pending_enqueued=45
pending_drained=45
pending_peak=1
irq_raised=45
irq_masked_count=45
irq_unmasked_count=45
napi_schedule_count=45
napi_poll_count=45
napi_complete_count=45
napi_budget_exhaust_count=0
```
**结论: ✅ NAPI 模式环回成功**

### 3. Direct 模式对比（rx_mode=direct）

```bash
sudo rmmod netdev_stage03
sudo insmod driver/netdev_stage03.ko rx_mode=direct
# 发送 5 帧
```
**debugfs 统计（Direct 模式）：**
```
rx_mode=direct
direct_inject_count=23
napi_inject_count=0
netif_rx_success=23
pending_enqueued=0
irq_raised=0
napi_schedule_count=0
```
**结论: ✅ Direct 模式环回成功，路径与 NAPI 模式完全不同**

### 4. Clone 模式验证（rx_mode=napi, loop_mode=clone）

```bash
sudo insmod driver/netdev_stage03.ko rx_mode=napi loop_mode=clone
# 发送 10 帧
```
**debugfs 统计（Clone 模式）：**
```
loop_mode=clone
copy_built=0
clone_built=22
```
**结论: ✅ Clone 模式成功**

### 5. Burst 场景（pending queue 积压测试）

- napi_weight=4，发送 30 帧
- 结果：pending_peak=1，未出现明显积压
- 原因：每帧触发独立 NAPI 周期，poll 清理速度快于帧到达速度
- **注**：教学模型中 budget exhaustion 需要更快速的批量到达场景才能观测

## 调用链对比

### Direct 模式
```
ndo_start_xmit()
  → stage03_build_rx_skb()
  → netif_rx(rx_skb)          ← 直接注入
  → stage03_note_direct_inject()
```

### NAPI 模式
```
ndo_start_xmit()
  → stage03_build_rx_skb()
  → skb_queue_tail(pending_rxq) ← 入队
  → stage03_raise_irq()
  → napi_schedule_prep()
  → __napi_schedule()
  → stage03_napi_poll(budget)   ← poll drain
    → skb_dequeue()
    → netif_receive_skb()
  → napi_complete_done()        ← 完成
```

## 关键指标解释

| 指标 | Direct 模式 | NAPI 模式 |
|------|-------------|-----------|
| 注入路径 | netif_rx() | netif_receive_skb() |
| pending queue | 不使用 | 入队/出队 |
| irq_raised | 0 | 每帧触发 |
| napi_schedule_count | 0 | 每帧触发 |
| napi_complete_count | 0 | 每帧触发 |
| poll 上下文 | 无 | 有 |

## 最终结论
**SMOKE TEST: PASS ✅**

| 测试项 | 状态 |
|--------|------|
| make build-userspace | ✅ |
| make build-module | ✅ (需 gcc-12 + netif_napi_add_weight) |
| insmod netdev_stage03 | ✅ |
| ip link show nds3 | ✅ 接口注册 |
| rx_mode=napi 环回 | ✅ napi_inject_count 增长 |
| rx_mode=direct 环回 | ✅ direct_inject_count 增长 |
| loop_mode=clone | ✅ clone_built 增长 |
| pending_rxq 入队/出队 | ✅ pending_enqueued/drained 匹配 |
| irq raise/mask/unmask | ✅ irq_*_count 正常 |
| napi poll/complete | ✅ napi_poll_count 正常 |
| debugfs stats 导出 | ✅ |
| netif_napi_add_weight API | ✅ 修复完成 |

## 遗留说明

- BTF generation skipped（无 vmlinux），不影响功能
- module taint warning（签名缺失），预期行为（测试环境）
- budget exhaustion 在教学模型中难以触发，需要真实高速流量
