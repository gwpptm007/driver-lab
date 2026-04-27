# PACKAGE_NOTES

## 本包新增

本次新增并完善：

```text
track-dpdk/lab-vhost-user-basic
```

主要内容：

- `scripts/00_check_env.sh`：环境检查
- `scripts/01_setup_hugepages.sh`：hugepage 配置
- `scripts/02_run_vhost_testpmd.sh`：启动 testpmd vhost-user backend
- `scripts/03_collect_stats.sh`：收尾状态采集
- `scripts/04_make_review_bundle.sh`：生成评审摘要
- `scripts/05_clean_runtime.sh`：清理残留 socket
- `docs/`：目标、流程、验收、原理、排障说明
- `reports/`：执行看板与报告

## 上一站收口

同时根据测试结果修正 `lab-vmxnet3-testpmd`：

- 默认 driver 调整为 `uio_pci_generic`，匹配 VMware Workstation guest 实际条件。
- `02_bind_vmxnet3.sh` bind 前清空 `BIND_AFTER.txt`，避免旧错误污染新记录。
- 新增 `docs/07_REVIEW_AFTER_TEST.md`。
- 更新执行看板为 PASS/PASS_WITH_NOTE。
