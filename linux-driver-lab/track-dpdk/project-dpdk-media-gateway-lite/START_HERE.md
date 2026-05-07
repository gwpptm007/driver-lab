# START_HERE - project-dpdk-media-gateway-lite

## 第一步：环境与构建

```bash
cd track-dpdk/project-dpdk-media-gateway-lite
./scripts/00_check_env.sh
./scripts/01_build_app.sh
```

## 第二步：优先跑 vdev 双端口 smoke

这条路径不依赖物理双网卡：

```bash
sudo ./scripts/05_run_vdev_null_pair_smoke.sh
./scripts/07_collect_stats.sh
./scripts/08_make_review_bundle.sh
```

重点看：

```text
MEDIA_GATEWAY_VDEV_NULL_PAIR.log
COMPARE_STATS.txt
REVIEW_BUNDLE.md
```

## 第三步：rewrite 配置 smoke

```bash
sudo ./scripts/06_run_rule_rewrite_demo.sh
./scripts/07_collect_stats.sh
./scripts/08_make_review_bundle.sh
```

这一步主要验证规则配置、rewrite 参数解析和记录结构。

## 第四步：真实 vmxnet3 单口 smoke

```bash
sudo ./scripts/02_prepare_vmxnet3.sh
sudo ./scripts/03_run_single_port_smoke.sh
```

单口环境下 `tx=0` 是正常的，重点是端口初始化、poll loop、stats 输出。

## 后续

如果 `project-fastpath-traffic-test` 已经能打出真实 UDP 流量，可以把同样的发包路径接到本项目上，推进到 `PASS_TRAFFIC / PASS_REWRITE / PASS_FORWARDING`。
