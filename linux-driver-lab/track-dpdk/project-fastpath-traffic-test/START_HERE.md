# START_HERE - project-fastpath-traffic-test

## 目标

把 `project-user-space-fastpath` 从 `PASS_SMOKE` 推进到：

```text
PASS_PCAP_FUNCTIONAL / PASS_PCAP_REWRITE / PASS_PCAP_FORWARDING
```

## 当前状态

| 等级 | 状态 | 说明 |
|------|------|------|
| `PASS_SMOKE` | ✅ 已验证 | fastpath-lite 启动、stats 打印正常 |
| `PASS_PCAP_FUNCTIONAL` | ✅ 已验证 | pcap PMD 输入，IPv4/UDP 计数非零 |
| `PASS_PCAP_REWRITE` | ✅ 已验证 | pcap 输入 + rewrite=1，rewrite 计数非零 |
| `PASS_PCAP_FORWARDING` | ✅ 已验证 | net_pcap RX 到 net_null TX 计数闭环 |

## 推荐测试路径（pcap PMD — 无需物理网卡）

```bash
cd track-dpdk/project-fastpath-traffic-test
./scripts/01_build_fastpath.sh

# PASS_TRAFFIC + PASS_FORWARDING (UDP packets via pcap PMD)
./scripts/06_run_pcap_rx_test.sh

# PASS_REWRITE (same, with rewrite enabled)
REWRITE_ENABLE=1 ./scripts/06_run_pcap_rx_test.sh
```

## 物理网卡测试路径（需要 vmxnet3 + 外部发包源）

```bash
cd track-dpdk/project-fastpath-traffic-test
./scripts/00_check_env.sh
./scripts/01_build_fastpath.sh
sudo ./scripts/02_prepare_vmxnet3.sh
sudo ./scripts/03_run_fastpath_rx.sh
./scripts/07_compare_stats.sh
./scripts/08_make_review_bundle.sh
```

## 外部发包

当前测试机的 DPDK 口是 `ens192/0000:0b:00.0`。一旦绑定到 `uio_pci_generic`，Linux 内核不能再通过 `ens192` 发包，所以真实流量建议来自：

1. 另一台 VM；或
2. 宿主机同网段接口；或
3. 后续 vhost/virtio-user 拓扑。

生成外部发包参考命令：

```bash
./scripts/04_send_udp_traffic.sh --print-only
```

## 通过标准

优先看：

```text
records/*/FASTPATH_RX.log
records/*/COMPARE_STATS.txt
records/*/REVIEW_BUNDLE.md
```

如果 `rx=0`，只能判定为 `PASS_SMOKE`，不能判定为 `PASS_TRAFFIC`。
