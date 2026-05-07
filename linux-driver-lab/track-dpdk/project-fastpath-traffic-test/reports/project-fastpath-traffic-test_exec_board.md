# project-fastpath-traffic-test_exec_board

| Step | Command | Expected | Status |
|---|---|---|---|
| 1 | `./scripts/00_check_env.sh` | 环境、工具、fastpath binary 状态 | TODO |
| 2 | `./scripts/01_build_fastpath.sh` | 复用上一站构建 fastpath-lite | TODO |
| 3 | `sudo ./scripts/02_prepare_vmxnet3.sh` | hugepage + vmxnet3 bind | TODO |
| 4 | `sudo ./scripts/03_run_fastpath_rx.sh` | port start + stats | TODO |
| 5 | 外部发包 | rx/ipv4/udp 非 0 | TODO |
| 6 | `./scripts/07_compare_stats.sh` | 自动判定 PASS 级别 | TODO |
| 7 | `./scripts/08_make_review_bundle.sh` | 生成复盘包 | TODO |
