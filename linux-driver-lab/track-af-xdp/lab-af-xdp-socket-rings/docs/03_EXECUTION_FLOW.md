# 03_EXECUTION_FLOW

## 执行流程

```bash
./scripts/00_check_env.sh
./scripts/01_build_app.sh
sudo AF_XDP_CONFIRM_REBIND=YES ./scripts/02_prepare_kernel_netdev.sh
sudo ./scripts/03_run_af_xdp_socket_smoke.sh
./scripts/05_collect_stats.sh
./scripts/06_make_review_bundle.sh
```

## 流量验证

smoke 阶段即使没有流量，只要看到：

```text
UMEM_READY
XSK_SOCKET_READY
FILL_RING_READY
XDP_ATTACHED
XSKMAP_REGISTERED
AF_XDP_RINGS_READY
```

就可以说明 socket/ring 初始化链路已经通。

要升级到 `PASS_RX_TRAFFIC`，需要在程序运行期间向测试网卡送包，并看到：

```text
AF_XDP_FINAL_STATS rx_packets=N
```

其中 `N > 0`。
