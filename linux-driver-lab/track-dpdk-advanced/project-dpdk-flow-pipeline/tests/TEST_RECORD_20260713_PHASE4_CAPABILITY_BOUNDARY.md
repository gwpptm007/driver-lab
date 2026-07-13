# TEST_RECORD_20260713_PHASE4_CAPABILITY_BOUNDARY

## 1. 目标

独立记录 Phase 4 的 RSS/RETA 与 hardware `rte_flow` 能力探测，区分“测试路径已执行”和“当前 PMD 不支持硬件能力”，避免把软件双 worker 结果误写成 NIC 多队列结果。

## 2. 环境

- 主机：`192.168.65.135`
- DPDK：`21.11.9`
- 输入 PMD：`net_pcap`
- 输出 PMD：`net_null`
- EAL：`--no-pci --no-huge`
- pcap PMD 角色：确定性报文输入，不代表真实 NIC

## 3. 完整命令

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk-advanced/project-dpdk-flow-pipeline
make clean
make
make test
grep -E 'FLOW_PORT_CAPABILITY|RSS_MULTI_QUEUE|RTE_FLOW_' \
  tests/runtime/flow_pipeline.log
```

应用内部执行：

```text
rte_eth_dev_info_get(port_id, &info)
rte_flow_validate(port_id, &attr, pattern, actions, &error)
```

`rte_flow_validate()` 使用 ingress、IPv4/UDP pattern 和 DROP action，只验证 PMD 接口，不创建规则，因此不会改变后续 64 包软件流水线的动作计数。

## 4. 实测结果

```text
FLOW_PORT_CAPABILITY max_rx_queues=1 max_tx_queues=0 reta_size=0 rss_offloads=0x0
RSS_MULTI_QUEUE_BOUNDARY_BLOCKED
RTE_FLOW_BOUNDARY_BLOCKED ret=-38 type=1 message=Function not implemented
```

解释：

- `max_rx_queues=1`：不能建立两个硬件 RX queue。
- `reta_size=0`：没有可配置的 RSS indirection table。
- `rss_offloads=0x0`：当前 PMD 不报告 RSS hash offload。
- `ret=-38`：Linux `ENOSYS`，对应 PMD 未实现该 `rte_flow` 操作。

## 5. 判定

```text
BOUNDARY_PCAP_RSS_RTE_FLOW
```

这不是代码编译失败或软件 pipeline 失败。探测调用、错误对象和 marker 均正常工作，但当前 PMD 无法提供硬件 steering 证据，因此 Phase 4 保持 capability boundary。

## 6. 真实 NIC 复验命令模板

```bash
sudo ./app/build/dpdk-flow-pipeline \
  -l 0-2 -n 4 -a <PCI_BDF> -- \
  --burst-size 32 --expected-packets 4096

grep -E 'FLOW_PORT_CAPABILITY|RSS_MULTI_QUEUE|RTE_FLOW_' <run.log>
```

复验必须补充 PCI BDF、PMD/固件版本、NUMA node、RX queue 数、RSS hash fields、RETA 映射，以及 `rte_flow` validate/create/query/destroy 的实际结果。只有报文真实分布到多个 NIC RX queue，才能把 RSS 状态从 boundary 改为 PASS。
