# lab-af-xdp-socket-rings exec board

| Step | Command | Expected |
|---|---|---|
| 00 | `./scripts/00_check_env.sh` | env record generated |
| 01 | `./scripts/01_build_app.sh` | `BUILD_RESULT=PASS` |
| 02 | `sudo AF_XDP_CONFIRM_REBIND=YES ./scripts/02_prepare_kernel_netdev.sh` | iface on kernel driver |
| 03 | `sudo ./scripts/03_run_af_xdp_socket_smoke.sh` | socket/rings ready |
| 04 | `./scripts/04_run_af_xdp_rx_with_traffic_hint.sh` | traffic hints generated |
| 05 | `./scripts/05_collect_stats.sh` | stats collected |
| 06 | `./scripts/06_make_review_bundle.sh` | verdict generated |
