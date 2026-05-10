# lab-xdp-redirect-basics exec board

| Step | Script | Expected |
|---|---|---|
| 0 | `00_check_env.sh` | toolchain / iface / driver info collected |
| 1 | `01_build_app.sh` | BPF object and loader built |
| 2 | `02_prepare_kernel_netdev.sh` | test iface is controlled by kernel driver |
| 3 | `03_run_xdp_pass.sh` | XDP_PASS attach + stats + detach |
| 4 | `04_run_xdp_drop.sh` | explicit DROP test |
| 5 | `05_run_xdp_redirect_dryrun.sh` | optional redirect model dry-run |
| 6 | `06_collect_stats.sh` | interface counters collected |
| 7 | `07_make_review_bundle.sh` | review bundle generated |
