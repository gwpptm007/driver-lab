# 02_ACCEPTANCE

## 验收等级

### 最低通过 (PASS_SMOKE)

- 第一个 Lab 完成
- 有 records
- 有 report

### 标准通过 (PASS_LAB)

- 单个 Lab 全部步骤执行完成
- REVIEW_BUNDLE.md 生成
- SUMMARY.md / RESULT.md 填写完整
- 能讲清本 Lab 和前一阶段的关系

### 优秀通过 (PASS_PROJECT)

- 全部 Lab 完成
- project 收口
- 有可运行的 fastpath-lite 代码
- 能作为简历/面试/项目展示材料

## 各 Lab 通过标准

### lab-vmxnet3-testpmd

| 检查项 | 证据 |
|--------|------|
| dpdk-devbind status | 0000:0b:00.0 drv=uio_pci_generic |
| hugepage | HugePages_Total > 0 |
| testpmd 启动 | Port 0 started, MAC 正常 |
| --no-pci | 命令中存在 --no-pci |
| stats 输出 | NIC statistics 正常 |

### lab-vhost-user-basic

| 检查项 | 证据 |
|--------|------|
| socket 创建 | socket_ready=1 |
| testpmd 启动 | EAL 正常，Port 0 初始化 |
| --no-pci | 命令中存在 --no-pci |

### lab-virtio-user-vhost

| 检查项 | 证据 |
|--------|------|
| backend socket | socket_ready=1 |
| frontend 连接 | frontend testpmd 启动成功 |
| 两边 stats | TESTPMD_BACKEND/FRONTEND.log 有输出 |

### lab-dpdk-l2-forwarding

| 检查项 | 证据 |
|--------|------|
| 编译成功 | BUILD.log 有 Linking target |
| EAL 正常 | IOVA mode PA |
| port 启动 | port 0 started |
| forwarding loop | enter forwarding loop |
| stats 输出 | rte_eth_stats 正常 |

### project-user-space-fastpath

| 检查项 | 证据 |
|--------|------|
| 编译成功 | fastpath-lite binary 生成 |
| policy 配置 | promisc/udp_only/swap_mac 配置生效 |
| 分类统计 | arp/ipv4/udp/non_udp 计数器工作 |
| rewrite 规则 | --rewrite 参数可解析 |

---

## 后续项目通过标准

### project-fastpath-traffic-test

| 等级 | 证据 |
|---|---|
| `PASS_SMOKE` | `FASTPATH_RX.log` 含 port started / enter fastpath loop / stats / bye |
| `PASS_TRAFFIC` | `COMPARE_STATS.txt` 解析出 `rx > 0` 且 `ipv4 > 0` 或 `udp > 0` |
| `PASS_REWRITE` | rewrite 配置打开，且 `rewrite > 0` |
| `PASS_FORWARDING` | 双端口或 vhost/virtio-user 拓扑下 `rx > 0` 且 `tx > 0` |

### project-dpdk-media-gateway-lite

进入条件：`project-fastpath-traffic-test >= PASS_TRAFFIC`。

通过标准：

- 配置文件能驱动方向规则；
- UDP-only 路径可验证；
- rewrite 可验证；
- per-rule/per-port/drop reason stats 可验证；
- records/reports/interview notes 完整。

### project-dpdk-v17-legacy-review

通过标准：

- 完成 v17 到 modern DPDK 对照；
- 完成媒体面数据路径迁移说明；
- 完成面试讲法和简历项目描述。

