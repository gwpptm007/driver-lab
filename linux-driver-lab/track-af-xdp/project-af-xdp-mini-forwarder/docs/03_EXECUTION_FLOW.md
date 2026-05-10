# 03_EXECUTION_FLOW

## drop smoke

```bash
./scripts/00_check_env.sh
./scripts/01_build_app.sh
sudo AF_XDP_CONFIRM_REBIND=YES ./scripts/02_prepare_kernel_netdev.sh
sudo ./scripts/03_run_forwarder_drop_smoke.sh
./scripts/06_collect_stats.sh
./scripts/07_make_review_bundle.sh
```

## reflect smoke

```bash
sudo ./scripts/04_run_forwarder_reflect_smoke.sh
./scripts/06_collect_stats.sh
./scripts/07_make_review_bundle.sh
```

## 带流量提示

```bash
./scripts/05_run_forwarder_with_traffic_hint.sh
sudo AF_XDP_DURATION=30 ./scripts/04_run_forwarder_reflect_smoke.sh
```

在另一个终端或另一台机器发送：

```bash
ping <ens192 peer ip>
```

或者发送 UDP：

```python
import socket, time
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
for i in range(1000):
    s.sendto(b'af-xdp-test', ('<target-ip>', 9000))
    time.sleep(0.001)
```
