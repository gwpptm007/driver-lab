# track-real-driver Roadmap

## Phase 0：项目前知识层

状态：`REAL_DRIVER_FUNDAMENTALS_COMPLETE`（2026-07-14）。

交付内容：

- `docs/fundamentals/README.md` 与 15 个主题文档；
- driver model、bus、lifecycle、queue/DMA、RX/TX、virtio/e1000e、控制面、并发；
- 源码阅读、运行观测、patch 验证和项目知识映射；
- 自动文档审计、Linux 软件回归与测试记录。

## Phase 1：virtio_net source dive

状态：Round 1-3 已有记录。

验收：

- 能从 `virtio_driver`、probe、netdev ops 找到主骨架；
- TX/RX 图包含异步 completion/refill；
- feature、ethtool、XDP 映射有版本边界；
- 笔记能关联 netdev 教学 stage。

## Phase 2：runtime observation

状态：已有 idle/ping 测试记录。

后续复验重点：

- 当前内核与当前接口身份；
- tracepoint/function hook 可用性；
- queue、CPU、IRQ/NAPI 分布；
- 低开销聚合与完整恢复命令。

## Phase 3：minimal stats patch

状态：已有 patch 和 before/after 记录。

后续复验重点：

- 目标 kernel source clean apply/build；
- patched module/kernel 确实加载；
- stats name/count/data 顺序；
- 并发、reset、queue resize 与 hot-path 开销。

## Phase 4：e1000e comparison

状态：已有第一轮结构对照。

继续深化：

- PCI probe/error/remove 资源对称；
- descriptor、tail register、DMA 与 completion；
- MSI/MSI-X、interrupt moderation、PHY/link；
- 与 virtio_net 共性/差异的版本化矩阵。

## Phase 5：patch + trace capstone

状态：项目骨架和 `poll_count` patch 已有证据。

最终标准：

```mermaid
flowchart LR
    Source[source claim] --> Patch[minimal patch]
    Patch --> Build[clean build]
    Build --> Deploy[identity verified]
    Deploy --> Trace[runtime trace/stats]
    Trace --> Compare[before/after]
    Compare --> Boundary[risk and boundary]
```

## 推荐后续

该知识层收口后进入 `track-virtual-net` fundamentals，把 virtio queue 的 guest 侧理解扩展到 TAP/bridge/vhost/backend 的端到端路径。
