# project-dpdk-media-gateway-lite_exec_board

| Step | Command | Expected | Status |
|---|---|---|---|
| 1 | `./scripts/00_check_env.sh` | 环境与工具检查 | ✅ 2026-06-07 |
| 2 | `./scripts/01_build_app.sh` | 生成 `app/build/media-gateway-lite` | ✅ 2026-06-07 |
| 3 | `./scripts/06_run_pcap_rx_test.sh` | pcap PMD 真实流量 (PASS_TRAFFIC/FORWARDING/REWRITE) | ✅ 2026-06-07 |
| 4 | `sudo ./scripts/05_run_vdev_null_pair_smoke.sh` | vdev 双端口 smoke | ✅ 2026-06-07 |
| 5 | `sudo ./scripts/06_run_rule_rewrite_demo.sh` | rewrite 规则配置 smoke | READY |
| 6 | `./scripts/08_make_review_bundle.sh` | 生成复盘包 | READY |
| A | `sudo ./scripts/03_run_single_port_smoke.sh` | vmxnet3 单口 smoke | OPTIONAL |
| B | `sudo ./scripts/04_run_two_port_forwarding.sh` | 双口转发 | OPTIONAL |
