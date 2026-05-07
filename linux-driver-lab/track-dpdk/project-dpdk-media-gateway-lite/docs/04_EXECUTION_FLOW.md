# 04_EXECUTION_FLOW

## 推荐测试顺序

```bash
./scripts/00_check_env.sh
./scripts/01_build_app.sh
sudo ./scripts/05_run_vdev_null_pair_smoke.sh
./scripts/07_collect_stats.sh
./scripts/08_make_review_bundle.sh
```

## 真实 vmxnet3 单口

```bash
sudo ./scripts/02_prepare_vmxnet3.sh
sudo ./scripts/03_run_single_port_smoke.sh
```

## 双口转发

```bash
sudo DPDK_PCI_0=0000:0b:00.0 DPDK_PCI_1=0000:xx:yy.z ./scripts/04_run_two_port_forwarding.sh
```

## rewrite 配置 smoke

```bash
sudo ./scripts/06_run_rule_rewrite_demo.sh
```
