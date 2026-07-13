# ROADMAP_NEXT - track-dpdk 后续推进路线

> 当前决策：pcap PMD 功能流量、forwarding、rewrite 已完成；下一阶段只补外部流量、真实 NIC 和性能证据，不把 vdev replay 称为真实 NIC 验证。

## 当前状态

| 阶段 | 目录/文件 | 状态 | 说明 |
|---|---|---|---|
| 1 | `lab-vmxnet3-testpmd` | `PASS` | vmxnet3/uio/hugepage/testpmd stats 已跑通 |
| 2 | `lab-vhost-user-basic` | `PASS` | vhost-user backend socket/testpmd 已跑通 |
| 3 | `lab-virtio-user-vhost` | `PASS_WITH_WARN` | virtio-user + vhost-user 本机对接已跑通，按记录保留 warning |
| 4 | `lab-dpdk-l2-forwarding` | `PASS_SMOKE` | l2fwd-lite C app 能编译、启动、打印 stats；未证明真实转发 |
| 5 | `project-user-space-fastpath` | `PASS_SMOKE` | fastpath-lite 能编译、启动、初始化端口、打印软件 stats |
| 6 | `project-fastpath-traffic-test` | `PASS_PCAP_FUNCTIONAL/FORWARDING/REWRITE` | pcap PMD 测试通过 (2026-06-07)，软件路径计数非零 |
| 7 | `project-dpdk-media-gateway-lite` | `PASS_PCAP_FUNCTIONAL/FORWARDING/REWRITE` | pcap PMD 测试通过 (2026-06-07)，rule/rewrite 闭环 |
| 8 | `project-dpdk-v17-legacy-review` | `PASS_REVIEW` | 旧 DPDK v17 经验与 modern DPDK 对照、面试/简历材料 |
| 9 | `project-dpdk-track-summary/project-dpdk-track-summary/reports/final/DPDK_TRACK_REPORT.md` | `READY` | track 总结、作品线、简历素材、后续 backlog |

## 当前完成路线

```text
project-dpdk-v17-legacy-review(PASS_REVIEW)
  -> project-dpdk-track-summary/reports/final/DPDK_TRACK_REPORT.md(READY)
  -> project-dpdk-track-summary/reports/final/DPDK_INTERVIEW_NOTES.md(READY)
  -> project-dpdk-track-summary/reports/final/DPDK_RESUME_MATERIAL_FINAL.md(READY)
  -> media-gateway-lite: PASS_TRAFFIC / PASS_FORWARDING / PASS_REWRITE (pcap PMD, 2026-06-07) ✅
  -> fastpath-traffic-test: PASS_TRAFFIC / PASS_FORWARDING / PASS_REWRITE (pcap PMD, 2026-06-07) ✅
```

## 为什么先做 DPDK_TRACK_REPORT

DPDK track 已经形成了完整学习与项目路径：

```text
PMD 接管 -> vhost-user -> virtio-user -> 自写 l2fwd -> fastpath -> traffic-test -> media-gateway-lite -> v17 迁移复盘
```

在明确 pcap/vdev 证据边界后，当前已经具备：

```text
1. 可运行 DPDK 测试机环境
2. 物理/虚拟 PMD smoke 证据
3. 自写 C 数据面程序
4. 项目型 media gateway 骨架与 smoke 记录
5. v17 旧项目经验到现代 DPDK 的映射材料
```

所以当前最有价值的是把它收成一份 `track report`，用于：

```text
简历项目描述
面试讲解
后续补测清单
阶段复盘归档
```

## 后续仍要补的技术债

详见 `project-dpdk-track-summary/reports/final/DPDK_BACKLOG.md`。

```text
project-dpdk-media-gateway-lite:
  - ✅ PASS_TRAFFIC: rx_ipv4/rx_udp 非 0 (pcap PMD, 2026-06-07)
  - ✅ PASS_FORWARDING: tx/rule_hit 非 0 (pcap PMD, 2026-06-07)
  - ✅ PASS_REWRITE: rewrite_hit 非 0 (pcap PMD, 2026-06-07)

project-fastpath-traffic-test:
  - ✅ PASS_TRAFFIC: rx/ipv4/udp 非 0 (pcap PMD, 2026-06-07)
  - ✅ PASS_FORWARDING: tx 非 0 (pcap PMD, 2026-06-07)
  - ✅ PASS_REWRITE: rewrite 非 0 (pcap PMD, 2026-06-07)
  - 若有时间，补 vhost/virtio-user 或 vmxnet3 外部流量证据
```

## 当前推荐执行

```bash
cd track-dpdk
./project-dpdk-track-summary/scripts/00_make_track_report_bundle.sh
```
