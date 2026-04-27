# PACKAGE_NOTES

## 本次更新

基于最新 `driver-lab.zip`，落地 `track-dpdk/lab-vmxnet3-testpmd` 第一站执行包。

## 关键新增

### 文档

- `docs/04_TEST_MACHINE_ENV.md`
- `docs/05_HUGEPAGE_AND_BIND.md`
- `docs/06_TESTPMD_STATS_REVIEW.md`

### 脚本

- `scripts/common.sh`
- `scripts/00_check_env.sh`
- `scripts/01_setup_hugepages.sh`
- `scripts/02_bind_vmxnet3.sh`
- `scripts/03_run_testpmd.sh`
- `scripts/04_collect_stats.sh`
- `scripts/05_make_review_bundle.sh`

### 模板/报告

- `records/templates/COMMANDS_TEMPLATE.md`
- `records/templates/RESULT_TEMPLATE.md`
- `reports/lab-vmxnet3-testpmd_exec_board.md`

## 测试机默认值

```text
管理口: ens33 / e1000 / 192.168.65.135
DPDK口: ens192 / vmxnet3 / 0000:0b:00.0
默认驱动: vfio-pci
```

## 安全保护

- 不默认操作 `ens33`
- `bind/unbind` 必须显式传 `DPDK_CONFIRM_BIND=YES`
- 不在脚本中写入 sudo 密码
- 绑定前检查 `DPDK_PCI` 不等于管理网卡 PCI

## 本地静态检查

已对所有 shell 脚本执行：

```bash
bash -n scripts/*.sh
```

全部通过。
