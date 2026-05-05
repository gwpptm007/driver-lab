# PACKAGE_NOTES

## 本包新增

本次新增并完善：

```text
track-dpdk/lab-virtio-user-vhost
```

主要内容：

- `scripts/00_check_env.sh`：环境检查，确认 testpmd、hugepage、socket、进程状态。
- `scripts/01_setup_hugepages.sh`：复用本 track 的 hugepage 配置。
- `scripts/02_run_virtio_user_vhost_pair.sh`：同时拉起 vhost backend 与 virtio-user frontend 两个 testpmd 进程。
- `scripts/03_collect_stats.sh`：收尾采集 socket、hugepage、进程、dmesg 状态。
- `scripts/04_make_review_bundle.sh`：生成评审摘要。
- `scripts/05_clean_runtime.sh`：清理残留 socket 和本实验 file-prefix 相关运行痕迹。
- `docs/`：目标、流程、验收、原理、排障说明。
- `reports/`：执行看板与报告。

## 本实验和前后阶段关系

```text
lab-vmxnet3-testpmd
  ↓ 真实 PMD smoke test
lab-vhost-user-basic
  ↓ vhost-user backend socket smoke test
lab-virtio-user-vhost
  ↓ virtio-user frontend + vhost-user backend local pair
lab-dpdk-l2-forwarding
  ↓ 自写 DPDK C app
project-user-space-fastpath
```
