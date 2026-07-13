# Track-DPDK Test Flow

## 1. 测试分层

| 测试 | 是否修改系统 | 验证内容 |
|---|---|---|
| `check_docs.py` | 否 | 文档、链接、围栏、Mermaid 和状态一致性 |
| `track_pcap_regression.sh` | 否 | pcap/parser/forward/rewrite/null TX 功能路径 |
| 各 lab 真实端口脚本 | 是 | hugepage、PCI binding、PMD/真实端口 |

track 级自动回归默认不绑定 PCI，不操作管理口，不宣称真实 NIC 或性能证据。

## 2. 本地文档审计

Windows：

```powershell
cd linux-driver-lab/track-dpdk
py tests/check_docs.py
```

Linux：

```bash
cd linux-driver-lab/track-dpdk
bash tests/check_docs.sh
```

预期：

```text
DPDK_TRACK_DOC_AUDIT_PASS files=15 mermaid>=40 links=pass status=consistent
```

## 3. pcap 非破坏性回归

依赖：DPDK 21.11.9、pcap PMD、null PMD、Meson/Ninja、Python 3。

```bash
cd linux-driver-lab/track-dpdk
bash -n tests/*.sh
bash tests/track_pcap_regression.sh
```

开发阶段跳过 clean build：

```bash
SKIP_BUILD=1 bash tests/track_pcap_regression.sh
```

预期最终 marker：

```text
PASS_PCAP_FUNCTIONAL
PASS_PCAP_FORWARDING
PASS_PCAP_REWRITE
DPDK_TRACK_PCAP_REGRESSION_PASS
```

## 4. 结果解释

拓扑是：

```text
generated UDP pcap
-> net_pcap PMD infinite replay
-> fastpath/media gateway
-> net_null PMD
```

该测试证明 parser、rule、rewrite、mbuf ownership 和统计路径。它没有真实 NIC、外部 wire、PCIe DMA 或硬件 RSS，因此不能写成 `PASS_REAL_NIC_FORWARDING` 或 `PASS_PERFORMANCE`。

## 5. 记录要求

阶段收口时保存：

- DPDK 版本和主机环境。
- 完整命令。
- 三个 scoped PASS marker。
- fastpath/media gateway 解析统计。
- 明确的 pcap/vdev capability boundary。

本轮实测记录：`TEST_RECORD_20260713_KNOWLEDGE_AND_PCAP.md`。
