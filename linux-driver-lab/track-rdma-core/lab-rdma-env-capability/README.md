# lab-rdma-env-capability

Phase 1 用来确认 RDMA 学习路线的真实环境边界。

## 目标

- 检查 `ibv_devices`、`ibv_devinfo`、`rdma` 等工具是否存在。
- 检查系统是否发现 RDMA device。
- 检查内核是否支持 `rdma_rxe` Soft-RoCE。
- 生成 records 和报告，作为后续 verbs 实验的入口依据。

## 快速执行

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/lab-rdma-env-capability
bash scripts/00_collect_env.sh
bash scripts/01_collect_rdma_capability.sh
bash scripts/02_try_soft_roce_boundary.sh
bash scripts/03_generate_summary.sh
```

## 输出

- `records/<timestamp>/ENV_CHECK.log`
- `records/<timestamp>/RDMA_CAPABILITY.log`
- `records/<timestamp>/SOFT_ROCE_BOUNDARY.log`
- `records/<timestamp>/SUMMARY.md`
- `reports/phase1_rdma_env_capability_report.md`
