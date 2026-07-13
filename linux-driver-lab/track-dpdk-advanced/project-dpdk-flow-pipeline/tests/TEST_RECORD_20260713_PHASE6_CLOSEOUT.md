# TEST_RECORD_20260713_PHASE6_CLOSEOUT

## 1. 目标

验证非法参数、端口不足和 worker lcore 不足时应用明确拒绝运行、返回失败并完成资源清理；随后执行全量回归，完成当前环境收口。

## 2. 环境

- 主机：`192.168.65.135`
- 用户：`wq7`
- DPDK：`21.11.9`
- PMD：`net_pcap` + `net_null`
- EAL：`--no-pci --no-huge`
- 正常路径 lcores：`0-2`
- worker 不足注入：`0-1`

## 3. 完整命令

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk-advanced/project-dpdk-flow-pipeline
chmod +x scripts/*.sh tests/*.sh tools/*.py
bash -n scripts/*.sh tests/*.sh
python3 -m py_compile tools/gen_flow_pcap.py tools/parse_tuning_matrix.py
make clean
make
make test-boundary
make test-all
```

## 4. Phase 6 首轮结果

```text
FLOW_BOUNDARY_CASE_PASS name=expected_packets status=1 marker=FLOW_CONFIG_BOUNDARY_REJECT reason=expected_packets value=63
FLOW_BOUNDARY_CASE_PASS name=extra_rules status=1 marker=FLOW_CONFIG_BOUNDARY_REJECT reason=arguments
FLOW_BOUNDARY_CASE_PASS name=insufficient_ports status=1 marker=FLOW_PORT_BOUNDARY_REJECT available=1 required=2
FLOW_BOUNDARY_CASE_PASS name=insufficient_workers status=1 marker=FLOW_WORKER_BLOCKED available=1 required=2
DPDK_FLOW_PIPELINE_PHASE6_BOUNDARY_PASS
PASS: DPDK flow pipeline failure boundaries and cleanup
script_summary name=error_boundary_test status=pass
```

四个 case 均以状态 1 退出，各自日志均包含 `cleanup=complete result=fail`。构建使用 `-Wall -Wextra`，未发现 warning/error。

## 5. 全量回归结果

最终执行 `make test-all`，验收以下 marker：

```text
DPDK_FLOW_PIPELINE_PHASE1_PASS
DPDK_FLOW_PIPELINE_PHASE2_LIFECYCLE_PASS
DPDK_FLOW_PIPELINE_PHASE3_WORKER_PASS
DPDK_FLOW_PIPELINE_PHASE5_TUNING_PASS
DPDK_FLOW_PIPELINE_PHASE6_BOUNDARY_PASS
DPDK_FLOW_PIPELINE_CURRENT_ENV_COMPLETE
```

## 6. 结论

Phase 1-3、5、6 在 135 的 pcap PMD 环境闭环。Phase 4 的 RSS/RETA 与 hardware `rte_flow` 仍受 PMD 能力限制，保留 `BOUNDARY_PCAP_RSS_RTE_FLOW`，不纳入软件路径失败，也不声明硬件完成。
