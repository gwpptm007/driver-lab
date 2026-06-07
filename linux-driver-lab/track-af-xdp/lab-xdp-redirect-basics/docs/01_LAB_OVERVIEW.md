# 01_LAB_OVERVIEW — 目标、流程与验收

## 定位

`lab-xdp-redirect-basics` 是 AF_XDP track 的第一站。目标不是马上写 AF_XDP socket，而是先确认 XDP 基础设施可用：

```
BPF 程序可编译 → XDP 程序可 attach → action 可控制 → stats map 可观测 → XSKMAP redirect 模型可解释
```

## 范围

**范围内：**
- XDP_PASS / XDP_DROP / XDP_REDIRECT
- `BPF_MAP_TYPE_ARRAY` 配置 action
- `BPF_MAP_TYPE_PERCPU_ARRAY` 统计包数/字节数
- `BPF_MAP_TYPE_XSKMAP` 预留 AF_XDP socket 映射
- libbpf 用户态 loader
- records / review bundle

**范围外（后续 lab/project）：**
- 完整 AF_XDP socket 创建
- UMEM fill/completion/rx/tx rings
- zero-copy 判断
- 用户态转发器

## 执行流程

```text
00_check_env.sh           → 确认工具链、内核 BPF 支持、网卡状态
01_build_app.sh           → 编译 BPF + loader
[02_prepare_kernel_netdev.sh] → 仅 ens192 被 DPDK 占用时需要
03_run_xdp_pass.sh        → PASS 模式，验证 attach/stats/detach
04_run_xdp_drop.sh        → DROP 模式，验证 action 控制（需显式确认）
[05_run_xdp_redirect_dryrun.sh] → REDIRECT dry-run，验证 XSKMAP 代码路径
06_collect_stats.sh       → 收集网卡统计
07_make_review_bundle.sh  → 生成 REVIEW_BUNDLE.md
```

关键命令：

```bash
# PASS
sudo AF_XDP_IFACE=veth-xdp bash scripts/03_run_xdp_pass.sh

# DROP（需显式确认）
sudo AF_XDP_CONFIRM_DROP=YES AF_XDP_IFACE=veth-xdp bash scripts/04_run_xdp_drop.sh

# REDIRECT dry-run（需显式确认）
sudo AF_XDP_CONFIRM_REDIRECT=YES AF_XDP_IFACE=veth-xdp bash scripts/05_run_xdp_redirect_dryrun.sh

# 收集统计 + 生成报告
sudo AF_XDP_IFACE=veth-xdp bash scripts/06_collect_stats.sh
bash scripts/07_make_review_bundle.sh
```

## 验收标准

### PASS_BASIC

```text
BUILD.log 中 BPF object 编译成功
BUILD.log 中 xdp_loader 编译成功
XDP_PASS.log 中 attach 成功
XDP_PASS.log 中 stats 周期输出
XDP_PASS.log 中 detach 成功
REVIEW_BUNDLE.md 生成
```

### PASS_ACTION

```text
XDP_DROP.log 存在
XDP_DROP.log 中 action=drop
drop/pass stats 有输出
```

### REDIRECT_MODEL_READY

```text
BPF 程序包含 xsks_map
XDP_REDIRECT_DRYRUN.log 存在
文档说明没有 AF_XDP socket 时不判定 PASS_AF_XDP
```

### 不允许夸大

当前 lab 不能写成 AF_XDP socket 收包成功、zero-copy 成功、用户态 forwarder 成功——这些是后续 lab/project 的目标。

## 当前状态

- 测试日期: 2026-06-07
- 判定: **PASS_BASIC=YES, PASS_ACTION=YES, REDIRECT_MODEL_READY=YES**
- 测试拓扑: veth pair (veth-peer → veth-xdp)
- 流量: XDP_PASS 12 pkts / XDP_DROP 3 pkts / XDP_REDIRECT 3 pkts
- 记录: `records/20260607-132613-xdp-redirect-basics/`
