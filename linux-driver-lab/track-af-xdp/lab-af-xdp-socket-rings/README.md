# lab-af-xdp-socket-rings

> 第二站：AF_XDP socket / UMEM / rings 最小闭环。

## 定位

上一站 `lab-xdp-redirect-basics` 证明 XDP 程序可以 attach 到测试网卡。当前这一站开始进入 AF_XDP 用户态收包模型：

```text
NIC RX queue
    ↓
XDP program
    ↓ bpf_redirect_map(xsks_map, rx_queue_index)
AF_XDP socket
    ↓
UMEM + FILL / RX / TX / COMPLETION rings
    ↓
userspace polling loop
```

## 当前目标

- 编译 XDP redirect BPF 程序；
- 创建 AF_XDP UMEM；
- 创建 FILL / COMPLETION / RX / TX rings；
- 创建 AF_XDP socket；
- 注册 socket fd 到 `BPF_MAP_TYPE_XSKMAP`；
- 从 RX ring 接收 descriptor 并回收到 FILL ring；
- 输出 `AF_XDP_FINAL_STATS` 作为 records 证据。

## 推荐执行

```bash
cd track-af-xdp/lab-af-xdp-socket-rings

./scripts/00_check_env.sh
./scripts/01_build_app.sh
sudo AF_XDP_CONFIRM_REBIND=YES ./scripts/02_prepare_kernel_netdev.sh
sudo ./scripts/03_run_af_xdp_socket_smoke.sh
./scripts/05_collect_stats.sh
./scripts/06_make_review_bundle.sh
```

如果要验证真实 RX：

```bash
./scripts/04_run_af_xdp_rx_with_traffic_hint.sh
sudo AF_XDP_DURATION=30 ./scripts/03_run_af_xdp_socket_smoke.sh
```

然后在另一台 VM/宿主机向 `ens192` 所在网络发送 ping/UDP/ARP 流量。

## 通过等级

| 等级 | 含义 |
|---|---|
| `PASS_SOCKET_READY` | UMEM、socket、XSKMAP 注册成功 |
| `PASS_UMEM_RINGS` | FILL/RX/TX/COMPLETION ring 创建成功 |
| `PASS_RX_TRAFFIC` | AF_XDP socket 收到真实流量，`rx_packets > 0` |

没有外部流量时，第一轮可以先接受 `PASS_SOCKET_READY + PASS_UMEM_RINGS`。
