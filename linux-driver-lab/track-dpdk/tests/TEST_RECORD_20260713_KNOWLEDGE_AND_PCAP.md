# Track-DPDK Knowledge Layer and Pcap Regression Record

## 1. 结论

- 日期：2026-07-13
- 本地文档审计：PASS
- 远端主机：`wq7@192.168.65.135`
- DPDK：21.11.9
- 功能拓扑：pcap PMD -> application -> null PMD
- 最终结果：`DPDK_TRACK_PCAP_REGRESSION_PASS`

## 2. 完整命令

本地文档检查：

```powershell
cd E:\02_Learning\2026\gitcode\driver-lab
py linux-driver-lab/track-dpdk/tests/check_docs.py
```

135 语法与文档检查：

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk
bash -n tests/*.sh \
  project-fastpath-traffic-test/scripts/06_run_pcap_rx_test.sh \
  project-dpdk-media-gateway-lite/scripts/06_run_pcap_rx_test.sh
python3 -m py_compile tests/check_docs.py
bash tests/check_docs.sh
```

135 clean build + pcap 功能矩阵：

```bash
bash tests/track_pcap_regression.sh \
  2>&1 | tee /tmp/track-dpdk-pcap-regression.log
```

## 3. 文档审计结果

```text
DPDK_TRACK_DOC_AUDIT_PASS files=15 mermaid=42 links=pass status=consistent
```

检查覆盖 fundamentals 必需文件、相对链接、Markdown 代码围栏、Mermaid 数量，以及 README 与状态真源中的 scoped marker。

## 4. Pcap 回归结果

fastpath 普通路径：

```text
rx=71926144 tx=71926144 ipv4=71926144 udp=71926144
rewrite=0 tx_failed=0
```

fastpath rewrite 路径：

```text
rx=47277504 tx=47277504 ipv4=47277504 udp=47277504
rewrite=47277504 tx_failed=0
```

media gateway rewrite 路径的最终统计满足：

```text
PASS_TRAFFIC=YES
PASS_FORWARDING=YES
PASS_REWRITE=YES
```

track scoped marker：

```text
PASS_PCAP_FUNCTIONAL
PASS_PCAP_FORWARDING
PASS_PCAP_REWRITE
DPDK_TRACK_PCAP_REGRESSION_PASS
```

## 5. 证据边界

测试使用 `--no-pci --no-huge`、`net_pcap` infinite replay 和 `net_null`。大计数来自软件无限回放，只证明 parser、rule、rewrite、mbuf ownership、TX 提交和统计路径；不证明外部 wire、真实 NIC DMA、RSS、Mpps/Gbps 或尾延迟。

## 6. 系统影响

- 未绑定或解绑任何 PCI 设备。
- 未修改管理口、route 或 IP。
- 未配置 hugepage。
- runtime 输出位于 `tests/runtime/`，由 `.gitignore` 排除。
