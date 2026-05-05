# PACKAGE_NOTES

本包从 `track-dpdk-lab-dpdk-l2-forwarding.zip` 继续推进，新增/补强：

```text
track-dpdk/project-user-space-fastpath/
├── app/                 # fastpath-lite C 程序
├── configs/             # 测试机和 rewrite 示例配置
├── docs/                # 架构、执行、验收、故障排查、面试讲解
├── scripts/             # 构建、绑定、运行、收集、review bundle
├── records/             # 记录模板
└── reports/             # 报告与执行看板
```

当前版本定位：

- 可以在 VMware 单 VMXNET3 DPDK 口上做 `PASS_SMOKE`
- 可以用 `net_null` vdev 做双端口和 rewrite 参数 smoke
- 后续接双口/外部流量源后进入 `PASS_FORWARDING`
