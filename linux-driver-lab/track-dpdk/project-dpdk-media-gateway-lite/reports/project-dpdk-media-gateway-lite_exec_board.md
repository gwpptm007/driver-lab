# project-dpdk-media-gateway-lite_exec_board

| Step | Command | Expected | Status |
|---|---|---|---|
| 1 | `./scripts/00_check_env.sh` | 环境与工具检查 | TODO |
| 2 | `./scripts/01_build_app.sh` | 生成 `app/build/media-gateway-lite` | TODO |
| 3 | `sudo ./scripts/05_run_vdev_null_pair_smoke.sh` | vdev 双端口 smoke | TODO |
| 4 | `sudo ./scripts/06_run_rule_rewrite_demo.sh` | rewrite 规则配置 smoke | TODO |
| 5 | `./scripts/07_collect_stats.sh` | stats 解析 | TODO |
| 6 | `./scripts/08_make_review_bundle.sh` | 生成复盘包 | TODO |
| A | `sudo ./scripts/03_run_single_port_smoke.sh` | vmxnet3 单口 smoke | OPTIONAL |
| B | `sudo ./scripts/04_run_two_port_forwarding.sh` | 双口转发 | OPTIONAL |
