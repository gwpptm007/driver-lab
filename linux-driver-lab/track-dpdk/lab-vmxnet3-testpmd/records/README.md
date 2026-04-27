# records

本目录保存 `lab-vmxnet3-testpmd` 的测试机执行证据。

推荐由脚本自动生成：

```bash
./scripts/00_check_env.sh
sudo ./scripts/01_setup_hugepages.sh
./scripts/02_bind_vmxnet3.sh status
sudo DPDK_CONFIRM_BIND=YES ./scripts/02_bind_vmxnet3.sh bind
sudo ./scripts/03_run_testpmd.sh
./scripts/04_collect_stats.sh
./scripts/05_make_review_bundle.sh
```

目录格式：

```text
records/<timestamp>-vmxnet3-testpmd/
```
