# Track-DPDK-Advanced Test Flow

## 1. 测试目标

```text
文档结构与链接
-> Phase 7 状态一致性
-> flow contract/actions/lifecycle
-> dual worker/ring ownership
-> tuning matrix
-> capability/error boundary
```

软件回归使用 pcap/null PMD，不绑定真实 PCI，不证明 RSS/RETA、VFIO/IOMMU 或硬件 `rte_flow`。

## 2. 本地文档审计

Windows：

```powershell
cd linux-driver-lab/track-dpdk-advanced
py tests/check_docs.py
```

Linux：

```bash
bash tests/check_docs.sh
```

预期：

```text
DPDK_ADVANCED_DOC_AUDIT_PASS files=10 mermaid>=25 links=pass phase7=consistent
```

## 3. 当前环境软件回归

```bash
cd linux-driver-lab/track-dpdk-advanced
bash -n tests/*.sh
bash tests/software_regression.sh
```

开发阶段保留现有 build：

```bash
SKIP_CLEAN=1 bash tests/software_regression.sh
```

预期最终 marker：

```text
DPDK_FLOW_PIPELINE_CURRENT_ENV_COMPLETE
DPDK_ADVANCED_KNOWLEDGE_AND_SOFTWARE_REGRESSION_PASS
```

## 4. 覆盖与边界

| 覆盖 | 结果类型 |
|---|---|
| flow key/hash/action | software functional |
| add/update/delete/aging | single-thread lifecycle |
| dual worker/SP-SC ring | software multicore model |
| tuning matrix | method evidence |
| RSS/rte_flow probe | capability boundary |
| concurrent control-plane delete | 未覆盖，RCU/QSBR 扩展 |
| real NIC RSS/RETA | 未覆盖，hardware branch |

## 5. 记录要求

- 主机、DPDK 和 PMD 版本。
- clean/skip-clean 选择。
- 文档审计 marker。
- `make test-all` 阶段 marker。
- capability blocked marker 与错误详情。
- 不把 pcap cycles/p99 包装成真实 NIC 端到端性能。

本轮实测记录：`TEST_RECORD_20260713_ADVANCED_KNOWLEDGE.md`。
