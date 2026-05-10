# START_HERE

这一站不要先追求高性能，也不要先做转发。目标是先把 AF_XDP 的四类 ring 跑通。

## 一键顺序

```bash
cd track-af-xdp/lab-af-xdp-socket-rings

./scripts/00_check_env.sh
./scripts/01_build_app.sh
sudo AF_XDP_CONFIRM_REBIND=YES ./scripts/02_prepare_kernel_netdev.sh
sudo ./scripts/03_run_af_xdp_socket_smoke.sh
./scripts/05_collect_stats.sh
./scripts/06_make_review_bundle.sh
```

## 关键日志

```text
records/*-af-xdp-socket-rings/ENV_CHECK.txt
records/*-af-xdp-socket-rings/BUILD.log
records/*-af-xdp-socket-rings/AF_XDP_SOCKET_SMOKE.log
records/*-af-xdp-socket-rings/COLLECT_STATS.txt
records/*-af-xdp-socket-rings/REVIEW_BUNDLE.md
```

## 关键字段

```text
UMEM_READY
XSK_SOCKET_READY
FILL_RING_READY
XDP_ATTACHED
XSKMAP_REGISTERED
AF_XDP_RINGS_READY
AF_XDP_FINAL_STATS
bye
```
