# Review Checklist

## 总入口检查

- [ ] `README.md` 能在 3 分钟内说明项目定位。
- [ ] `docs/07_FINAL_ARCHITECTURE.md` 能说明六条路径之间的关系。
- [ ] `reports/final_report.md` 能作为对外主报告阅读。
- [ ] `reports/resume_material.md` 能直接转成简历 bullet。
- [ ] `docs/08_INTERVIEW_SHARE_SCRIPT.md` 能支持 30 秒和 3 分钟讲法。

## Evidence 检查

- [ ] 每个 evidence 文件至少有 README 入口。
- [ ] 每个 evidence 文件至少有 report 入口。
- [ ] 每个 evidence 文件至少有 records 或测试结论入口。
- [ ] 所有相对路径存在。
- [ ] 测试结论没有夸大。

## 表达检查

- [ ] DPDK 不写成生产级媒体网关。
- [ ] AF_XDP 不写成真实 NIC zero-copy 压测完成。
- [ ] Real driver patch 不写成深度驱动重构。
- [ ] Virtual net 不写成完整云网络控制面。
- [ ] eBPF observability 不写成生产级平台。

## 技术追问准备

- [ ] 能解释 netdev 中 `skb`、NAPI、ring 的关系。
- [ ] 能解释 `virtio_net` 中 queue/NAPI/TX/RX 的对应源码位置。
- [ ] 能解释 tap、bridge、vhost、kick/notify 的协同。
- [ ] 能解释 DPDK PMD、EAL、mempool、rx_burst/tx_burst。
- [ ] 能解释 AF_XDP UMEM 和四个 rings。
- [ ] 能解释 eBPF 观测报告中的 RX/GRO/TX/drop 指标。

## 发布前建议

运行一次路径检查：

```powershell
rg "\.\./" linux-driver-lab\project-linux-network-data-plane
```

然后逐个打开关键链接，确认对外展示不会断链。
