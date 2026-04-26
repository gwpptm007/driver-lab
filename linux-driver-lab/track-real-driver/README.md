# track-real-driver

> stage14 之后的第一条正式 Track：真实 Linux 驱动源码与补丁线。

## 本 Track 包含四个 Lab + 一个收尾项目，按推荐顺序排列：

| # | Lab/Project | 定位 | 状态 |
|---|-------------|------|------|
| 1 | `lab-virtio-net-source-dive/` | 源码研究 — 理解 virtio_net 驱动结构 | ✅ 已完成 Round1~3 |
| 2 | `lab-virtio-net-runtime-observe/` | 运行时观测 — 验证 TX/RX 路径事件链 | ✅ 已测试 |
| 3 | `lab-virtio-net-ethtool-stats-mini-patch/` | 小补丁实验 — before/after 对照 | ✅ 已测试 |
| 4 | `lab-virtio-net-queue-poll-observe/` | NAPI poll 链观测 — napi_poll → netif_receive_skb | ✅ 已测试 |
| 5 | `project-virtio-net-patch-and-trace/` | 收尾小项目 — poll_count stat patch + trace 证据链 | ✅ patch 已生成 |

### 推荐推进顺序

先把 `virtio_net` 看懂，再进入更偏”验证/补丁”的实验：

```
lab-virtio-net-source-dive
  → lab-virtio-net-runtime-observe
    → lab-virtio-net-ethtool-stats-mini-patch
      → lab-virtio-net-queue-poll-observe
        → project-virtio-net-patch-and-trace (收尾项目)
```

### 各 Lab 说明

**1. lab-virtio-net-source-dive**
- 把教学驱动与 Linux 内核中的真实 `virtio_net` 驱动建立一一映射
- Round1：架构 / probe / netdev / queue / napi
- Round2：TX / RX / trace
- Round3：feature / ethtool / XDP / mapping

**2. lab-virtio-net-runtime-observe**
- 把 TX/RX 路径理解、queue/NAPI 事件推进理解、stats/workload 对照落成运行期证据
- Idle baseline vs ping workload 对照
- trace point: netif_receive_skb

**3. lab-virtio-net-ethtool-stats-mini-patch**
- 第一个真实小 patch
- 第一个 before/after 驱动实验
- 第一个低风险 control-plane/stats 实验

**4. lab-virtio-net-queue-poll-observe**
- 把 queue / callback / napi schedule / poll 事件链跑出运行期证据
- 为后续更细 tracing 或轻量观测 patch 选点
- trace point: napi_poll → netif_receive_skb 链

**5. project-virtio-net-patch-and-trace (收尾项目)**
- 整合前面四个 Lab 的成果，形成一个完整的 patch + trace 项目
- 选点：`virtnet_poll()` 入口添加 `poll_count` 统计
- 直接对应 `queue-poll-observe` 中观测到的 napi_poll 调用链
- 交付 patch 文件 + before/after 证据 + trace 说明

### 测试结果

```
lab-virtio-net-runtime-observe:
  Idle baseline: RX +6, 6 trace events
  Ping (20): RX +21, 21 trace events, 0% loss, RTT avg 0.593ms

lab-virtio-net-ethtool-stats-mini-patch:
  10 ping: RX +11, TX +11

lab-virtio-net-queue-poll-observe:
  Ping window (20): 63 trace events, RX +21, TX +22
  成功捕获 napi_poll → netif_receive_skb 链

project-virtio-net-patch-and-trace:
  Patch: virtio_net_poll_count.patch (3 行修改)
  新增 poll_count stat 到 virtnet_rq_stats / rq_stats_desc / virtnet_poll()
  记录目录: records/20260425_203801-virtio-net-patch-trace/
```
