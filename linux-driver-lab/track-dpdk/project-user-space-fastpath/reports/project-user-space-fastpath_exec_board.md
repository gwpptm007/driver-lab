# project-user-space-fastpath_exec_board

| Step | Command | Expected | Status |
|---|---|---|---|
| 1 | `./scripts/00_check_env.sh` | 生成 ENV_CHECK | READY |
| 2 | `./scripts/01_build_app.sh` | 生成 fastpath-lite | READY |
| 3 | `sudo ./scripts/02_prepare_vmxnet3.sh` | hugepage + bind vmxnet3 | READY |
| 4 | `sudo ./scripts/03_run_fastpath_single_port.sh` | 单端口 smoke | READY |
| 5 | `sudo ./scripts/05_run_fastpath_vdev_null_pair.sh` | vdev 双端口 smoke | OPTIONAL |
| 6 | `sudo ./scripts/06_run_fastpath_rewrite_demo.sh` | rewrite 参数路径 | OPTIONAL |
| 7 | `./scripts/07_collect_stats.sh` | 收集状态 | READY |
| 8 | `./scripts/08_make_review_bundle.sh` | 生成 REVIEW_BUNDLE | READY |

## 判定

- 第一轮目标：`PASS_SMOKE`
- 第二轮目标：`PASS_PROJECT`
- 接双口/外部流量后目标：`PASS_FORWARDING`
