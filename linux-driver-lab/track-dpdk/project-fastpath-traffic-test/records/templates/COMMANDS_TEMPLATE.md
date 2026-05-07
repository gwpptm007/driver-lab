# COMMANDS

```bash
cd track-dpdk/project-fastpath-traffic-test
./scripts/00_check_env.sh
./scripts/01_build_fastpath.sh
sudo ./scripts/02_prepare_vmxnet3.sh
sudo ./scripts/03_run_fastpath_rx.sh
./scripts/04_send_udp_traffic.sh --print-only
./scripts/07_compare_stats.sh
./scripts/08_make_review_bundle.sh
```
