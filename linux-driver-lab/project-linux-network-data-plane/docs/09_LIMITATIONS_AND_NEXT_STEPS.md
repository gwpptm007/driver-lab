# Limitations And Next Steps

## 当前边界

这个项目已经能作为 Linux 网络数据面作品集展示，但仍然是实验型项目，不是生产系统。

## 不能夸大的点

| 主题 | 准确说法 | 不应夸大为 |
|------|----------|------------|
| DPDK | pcap PMD 路径下 media-gateway-lite 已验证 UDP traffic/forwarding/rewrite | 生产级媒体网关 |
| AF_XDP | 已验证 XDP redirect、UMEM/rings、mini forwarder | 完整高性能 AF_XDP 转发器 |
| Real driver | 已做源码阅读、运行期观测、低风险 stats patch | 深度重构真实 NIC 驱动 |
| Virtual net | 已验证 tap/bridge/vhost/kick/notify 路径 | 完整云网络控制面 |
| eBPF | 已形成实验型观测报告 | 生产级可观测性平台 |

## P0 收口任务

这些任务用于让当前 project 更像一个最终作品，而不是材料集合：

1. 保持 `README.md`、`reports/final_report.md`、`reports/resume_material.md` 同步。
2. 每条 evidence 文档至少指向一个 README、一个 report、一个 records 入口。
3. 对外展示前检查所有链接是否存在。
4. 统一术语：fastpath、observability、netdev、real driver、virtual net。

## P1 补强方向

这些任务可以提高作品说服力：

| 方向 | 价值 |
|------|------|
| DPDK 双口真实网卡转发验证 | 从 pcap PMD 推进到更真实的数据面 |
| AF_XDP 性能基线 | 形成 copy/native/zero-copy 支持边界和性能对照 |
| DPDK vs AF_XDP 横向报告 | 强化 fastpath 体系理解 |
| eBPF + virtual net 联合观测 | 在 host/guest 路径上验证观测能力 |
| real driver patch before/after 完整截图或日志索引 | 提升 patch 可信度 |

## P2 扩展方向

更长期可以扩展：

- RSS / multi-queue 真实 NIC 验证。
- IRQ affinity / RPS / XPS 对照实验。
- tc/eBPF 或 XDP load balancer 小项目。
- OVS/OVN 或 vhost-user 与 DPDK 的联合路径。
- block layer / storage I/O 作为高性能 I/O 第二主线。

## 下一步推荐

当前最推荐的下一步不是开新 track，而是做一次展示级收口：

```text
1. 检查 evidence 链接。
2. 把 final_report.md 当作主材料打磨。
3. 把 resume_material.md 压缩成 2~3 条简历 bullet。
4. 用 interview_share_script.md 练 3 分钟讲法。
```
