# 06_auto_collect_flow - 宿主机自动采样流程

## 1. 基本命令

```bash
cd linux-driver-lab/day17/collect
SCENARIO_ID=day17-baseline-arm64-virt ./host_collect.sh
```

round1 / round2b 只需要换 scenario_id：

```bash
SCENARIO_ID=day17-round1-arm64-virt  ./host_collect.sh
SCENARIO_ID=day17-round2b-arm64-virt ./host_collect.sh
```

## 2. host_collect.sh 输出什么

默认会在：

```text
day17/records/<timestamp>-<scenario_id>/
```

生成：

- `serial.log`
- `host_metrics.env`
- `guest_metrics.env`
- `metrics.env`
- `baseline.csv`
- `meminfo.txt`
- `modules.txt`
- `available_tracers.txt`
- `dmesg_tail.txt`
- `snapshot.txt`
- `perf_version.txt`
- `perf_list.txt`
- `perf_stat.txt`
- `perf_manifest.txt`

## 3. 怎样判断 perf 自动采样成功

如果你这次 rootfs 里已经集成 perf，那么自动采样完成后应满足：

- `metrics.env` 里 `perf_bin_ok=yes`
- `metrics.env` 里 `perf_smoke_ok=yes`
- `perf_version.txt` 非空
- `perf_stat.txt` 非空

如果 `perf_bin_ok=yes` 但 `perf_smoke_ok=no`，优先看 `perf_stat.txt`。

## 4. 串口握手机制

`host_collect.sh` 会先做串口握手：

1. 先等 prompt
2. 再发一个空回车
3. 再发 `echo __DAY17_HOST_HANDSHAKE__`
4. 只有在 `serial.log` 中确认看到该 token 后，才继续执行 guest_collect

这样做的原因是：某些最小 rootfs / BusyBox / QEMU 组合里，虽然日志上已经看到了 prompt，
但第一条通过 FIFO 注入的命令仍然可能丢失。
