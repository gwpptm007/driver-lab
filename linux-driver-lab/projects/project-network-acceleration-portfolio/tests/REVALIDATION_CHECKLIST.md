# Revalidation Checklist

## 1. 使用方式

本清单用于把作品集中的“已有证据”复验到新的机器。每次复验应新建日期目录，保存命令、环境快照、原始日志和结论。不要覆盖已有记录，也不要把不同 NIC、内核、NUMA 拓扑或包长的结果放进同一张对比表。

建议目录：

```text
tests/records/YYYYMMDD-<topic>/
  environment.txt
  command.txt
  raw/
  summary.md
```

## 2. 统一环境快照

```bash
date -Is
uname -a
lscpu -e=CPU,NODE,SOCKET
numactl --hardware
ip -br link
lspci -nn
```

真实 NIC、SmartNIC 或 DPU 环境额外保存：

```bash
ethtool -i <interface>
ethtool -S <interface>
devlink dev info
devlink port show
```

验收：快照中能唯一识别内核、CPU/NUMA、NIC、驱动和固件。任何一项缺失时，性能数字只能作为临时观察。

## 3. RDMA 双机与 NUMA 复验

### 3.1 先决检查

```bash
ssh -o BatchMode=yes <host-a> true
ssh -o BatchMode=yes <host-b> true
ibv_devices
ibv_devinfo -v
rdma link show
```

验收：两端均可无交互登录，RDMA 设备与 link 状态正常。当前历史环境中 `192.168.65.134` 登录受阻，因此不应把单机 RXE 结果升级为双机 fresh 验证。

### 3.2 性能项目

在 `track-rdma-core/project-rdma-performance-tuning/` 中按 README 的 server/client 命令分别执行：

```bash
make clean
make
make envcheck
make test
```

矩阵至少包含：single SEND、batch SEND、inline 开关、selective signaling interval、CQ polling budget、同 NUMA node、跨 NUMA node。每一行固定消息大小、迭代数、CPU 绑定、NUMA 绑定、NIC 端口和运行方向。

验收：服务端和客户端日志都出现预期 PASS marker；数值表只比较同一环境字段完全一致的行。

## 4. DPDK 与 AF_XDP 复验

```bash
cd track-dpdk/project-user-space-fastpath
./scripts/01_build_app.sh

cd ../../track-af-xdp
rg -n "PASS|zero-copy|copy mode" README.md ROADMAP.md
```

pcap PMD 和 veth 可用于逻辑闭环；真实 NIC 才可用于 line-rate、RSS 多队列、AF_XDP zero-copy 和 NUMA 对比。必须把 PMD/driver、队列数、lcore/cpuset、IRQ affinity 和包长写进结果表。

## 5. SmartNIC / DPU 复验

执行前先读 `docs/06_SMARTNIC_DPU_MAP.md`，按 H0 到 H5 逐级推进。首轮只下发一条易识别的 `tc flower` 规则，保存下发前后 `tc -s`、`ethtool -S` 和 `devlink health`。

验收：规则明确标记为 `in_hw`，规则计数随测试流量增长，且 health reporter 没有新增故障。三项中任一项不满足时，结论只能写为“规则尝试下发”，不能写为“硬件 offload 已验证”。

## 6. 结论模板

```text
环境：kernel / NIC / driver / firmware / NUMA / CPU binding
工作负载：包长 / 迭代 / 队列 / 流量方向
命令：可直接重放的 server 与 client 命令
结果：原始 marker、统计值、对比基线
结论：已验证的行为
边界：未控制变量、软环境限制、待补硬件条件
```

完成一次复验后，将记录路径和 PASS/BLOCKED 状态添加到 `EVIDENCE_INDEX.md`，不要只更新结论文字。
