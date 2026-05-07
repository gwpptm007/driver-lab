# START_HERE - project-fastpath-traffic-test

## 目标

把 `project-user-space-fastpath` 从 `PASS_SMOKE` 推进到：

```text
PASS_TRAFFIC / PASS_REWRITE / PASS_FORWARDING
```

## 当前状态

| 等级 | 状态 | 说明 |
|------|------|------|
| `PASS_SMOKE` | ✅ 已验证 | fastpath-lite 启动、stats 打印正常 |
| `PASS_TRAFFIC` | ⏳ 待验证 | 需要外部 UDP 流量 |
| `PASS_REWRITE` | ⏳ 待验证 | 需要流量 + rewrite=1 |
| `PASS_FORWARDING` | ⏳ 待验证 | 需要双端口/vhost 拓扑 |

## 最小测试路径

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
