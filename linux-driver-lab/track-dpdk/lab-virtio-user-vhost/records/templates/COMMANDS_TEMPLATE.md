# COMMANDS_TEMPLATE

```bash
cd track-dpdk/lab-virtio-user-vhost
./scripts/00_check_env.sh
sudo ./scripts/01_setup_hugepages.sh
sudo ./scripts/02_run_virtio_user_vhost_pair.sh
./scripts/03_collect_stats.sh
./scripts/04_make_review_bundle.sh
```

可选调参：

```bash
sudo BACKEND_CORES=0-1 FRONTEND_CORES=2-3 \
     BACKEND_FORWARD_MODE=rxonly FRONTEND_FORWARD_MODE=txonly \
     ./scripts/02_run_virtio_user_vhost_pair.sh
```
