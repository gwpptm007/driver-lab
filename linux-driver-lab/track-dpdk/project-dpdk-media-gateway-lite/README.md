# project-dpdk-media-gateway-lite

> 简化版 DPDK 用户态媒体网关项目。

## 当前状态

```text
READY_TO_TEST / PROJECT_SMOKE ✅ (2026-05-07 vdev_null_pair PASS_SMOKE)
```

| 级别 | 状态 | 说明 |
|------|------|------|
| `PASS_BUILD` | ✅ | 编译成功 |
| `PASS_SMOKE` | ✅ | EAL/port/rule/poll/stats 跑通 |
| `PASS_RULE_CONFIG` | ⏳ | 需真实流量验证 |
| `PASS_TRAFFIC` | ⏳ | 需外部 UDP 发包 |
| `PASS_REWRITE` | ⏳ | 需流量 + rewrite=1 |

本项目承接前面的：

```text
project-user-space-fastpath
project-fastpath-traffic-test
```

目标不是再做一个 testpmd demo，而是把 DPDK 数据面整理成更接近工作项目的结构：

```text
EAL/port/mempool
  -> packet parse
  -> rule lookup
  -> UDP-only media fast path
  -> MAC/IP/UDP rewrite
  -> per-port/per-rule/drop stats
  -> records/review bundle
```

## 推荐执行

```bash
cd track-dpdk/project-dpdk-media-gateway-lite

./scripts/00_check_env.sh
./scripts/01_build_app.sh
sudo ./scripts/05_run_vdev_null_pair_smoke.sh
./scripts/07_collect_stats.sh
./scripts/08_make_review_bundle.sh
```

如果要走真实 vmxnet3 单口 smoke：

```bash
sudo ./scripts/02_prepare_vmxnet3.sh
sudo ./scripts/03_run_single_port_smoke.sh
./scripts/07_collect_stats.sh
./scripts/08_make_review_bundle.sh
```

如果测试机后续有两个 DPDK 口：

```bash
sudo DPDK_PCI_0=0000:0b:00.0 DPDK_PCI_1=0000:xx:yy.z ./scripts/04_run_two_port_forwarding.sh
```

## 验收级别

| 级别 | 含义 |
|---|---|
| `PASS_BUILD` | `media-gateway-lite` 编译成功 |
| `PASS_SMOKE` | EAL/port/mempool/rule/stats 跑通 |
| `PASS_RULE_CONFIG` | 双向规则与 rewrite 配置能打印并进入数据面 |
| `PASS_TRAFFIC` | rx/ipv4/udp 非 0，证明真实或虚拟流量进入 |
| `PASS_FORWARDING` | 双端口或虚拟拓扑下 tx 非 0 |
| `PASS_REWRITE` | rewrite_hit 非 0 |

## 和真实媒体面项目的对应关系

```text
运营商网元媒体面
  -> UDP-only 收包
  -> ARP/IPv4/UDP 解析
  -> MAC/IP/UDP 头改写
  -> 按方向转发
  -> 按规则/端口/drop reason 统计
```

本项目是上面能力的 lite 版本，适合用于学习、复盘和简历作品化。
