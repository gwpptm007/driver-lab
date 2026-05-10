# project-af-xdp-mini-forwarder

> AF_XDP 项目型 mini forwarder：把前面 XDP attach、XSKMAP redirect、UMEM/rings、copy/zero-copy mode 选择串成一个可运行的数据面小项目。

## 当前定位

这一站不是先追求高性能压测，而是先做一个结构清楚、可编译、可运行、可复盘的 AF_XDP mini forwarder：

- XDP BPF 程序把指定 RX queue redirect 到 XSKMAP；
- 用户态创建 UMEM 与 AF_XDP socket；
- 初始化 FILL / RX / TX / COMPLETION rings；
- 支持 `drop` 模式：收包后回收 frame，用于 RX smoke；
- 支持 `reflect` 模式：把收到的 frame 直接从同一个 XSK TX ring 发回，用于 TX/COMPLETION smoke；
- 支持 `skb/copy` 默认路径，保留 `native/zero-copy` 参数但允许环境不支持时失败；
- 输出明确 stats，生成 review bundle。

## 推荐执行

```bash
cd track-af-xdp/project-af-xdp-mini-forwarder

./scripts/00_check_env.sh
./scripts/01_build_app.sh
sudo AF_XDP_CONFIRM_REBIND=YES ./scripts/02_prepare_kernel_netdev.sh
sudo ./scripts/03_run_forwarder_drop_smoke.sh
./scripts/06_collect_stats.sh
./scripts/07_make_review_bundle.sh
```

reflect smoke：

```bash
sudo ./scripts/04_run_forwarder_reflect_smoke.sh
./scripts/06_collect_stats.sh
./scripts/07_make_review_bundle.sh
```

如果需要真实 RX/TX 统计非 0，运行期间需要从另一台 VM / 宿主机向 `AF_XDP_IFACE` 发送 ARP、ping 或 UDP 流量。

## 当前预期状态

| 项 | 状态 |
|---|---|
| build | 待测试机验证 |
| UMEM / XSK socket | 待测试机验证 |
| XSKMAP register | 待测试机验证 |
| drop smoke | 待测试机验证 |
| reflect smoke | 待测试机验证 |
| traffic forwarding | 后续补测 |
