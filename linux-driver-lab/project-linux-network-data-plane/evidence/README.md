# Evidence Index

本目录是 `project-linux-network-data-plane` 的证据索引层。

它不复制各 track 的原始材料，只负责把总项目中的六条路径指向已有的 README、reports、records 和测试结论。

## 索引文件

| 文件 | 对应路径 |
|------|----------|
| [netdev_evidence.md](netdev_evidence.md) | Kernel netdev |
| [real_driver_evidence.md](real_driver_evidence.md) | Real driver |
| [virtual_net_evidence.md](virtual_net_evidence.md) | Virtual network |
| [dpdk_evidence.md](dpdk_evidence.md) | DPDK fastpath |
| [af_xdp_evidence.md](af_xdp_evidence.md) | AF_XDP path |
| [ebpf_observability_evidence.md](ebpf_observability_evidence.md) | eBPF observability |

## 使用方式

对外评审时，先看：

```text
../README.md
../reports/final_report.md
../docs/07_FINAL_ARCHITECTURE.md
```

需要核验证据时，再进入本目录对应文件。

## 证据标准

每条路径尽量包含：

- 项目或 track README。
- 阶段报告。
- records 或测试记录。
- 明确的 PASS / COMPLETED / REVIEW 结论。
- 当前边界说明。
