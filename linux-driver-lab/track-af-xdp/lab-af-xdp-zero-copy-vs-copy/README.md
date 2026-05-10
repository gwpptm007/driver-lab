# lab-af-xdp-zero-copy-vs-copy

> AF_XDP copy mode 与 zero-copy mode 支持边界实验。

## 目标

上一站 `lab-af-xdp-socket-rings` 解决了 AF_XDP socket、UMEM、FILL/RX/TX/COMPLETION rings 的基本创建。  
这一站继续比较不同模式：

```text
skb + copy             兼容性基线
native + copy          native XDP attach 能力探测
native + zero-copy     zero-copy 能力探测
skb + zero-copy        反例/边界探测
```

## 为什么要单独做这一站

AF_XDP 不是打开一个 `--zero-copy` 参数就一定 zero-copy。zero-copy 依赖：

- 网卡驱动是否支持 native XDP；
- 驱动是否支持 AF_XDP ZC；
- 队列、UMEM、ring 参数是否满足要求；
- 当前网卡是否是虚拟网卡、物理网卡或 veth；
- 内核版本与 libbpf/xsk API 行为。

你的 VMware 测试机当前是 `vmxnet3/ens192`，很可能只能稳定验证 `skb + copy`，`native/zero-copy` 失败也属于有价值记录。

## 推荐执行

```bash
cd track-af-xdp/lab-af-xdp-zero-copy-vs-copy

./scripts/00_check_env.sh
./scripts/01_build_app.sh
sudo AF_XDP_CONFIRM_REBIND=YES ./scripts/02_prepare_kernel_netdev.sh
sudo ./scripts/03_run_copy_mode_baseline.sh
sudo ./scripts/04_probe_native_copy.sh
sudo ./scripts/05_probe_zero_copy.sh
./scripts/06_compare_modes.sh
./scripts/07_collect_stats.sh
./scripts/08_make_review_bundle.sh
```

## 通过标准

最低通过：

```text
PASS_BUILD
PASS_COPY_BASELINE 或明确记录 COPY_BASELINE_FAIL
ZERO_COPY_PROBED
COMPARE_MODES.txt 存在
REVIEW_BUNDLE.md 存在
```

更高等级：

```text
PASS_NATIVE_COPY
PASS_ZERO_COPY
```

如果 `zero-copy` 在 `vmxnet3` 上失败，不视为项目失败；只要记录清楚失败原因和 fallback 到 copy mode 的策略即可。
