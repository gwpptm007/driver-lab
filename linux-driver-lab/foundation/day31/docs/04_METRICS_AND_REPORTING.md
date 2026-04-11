# day31 指标与报表说明

## 1. 为什么不能只报平均值

平均值太容易掩盖抖动。尤其在 DMA 或虚拟化环境里，尾延迟往往比平均值更能说明问题。

因此 Day31 当前至少输出：

- `avg_us`
- `p50_us`
- `p95_us`
- `p99_us`
- `min_us`
- `max_us`

## 2. 吞吐的解释方式

当前实现的 `throughput_mbps` 按“逻辑 payload bytes / wall time”计算。

这意味着：

- `bench-mmap` 更像观察用户态映射区内的数据处理效率
- `bench-dma` 更像观察“为了得到同样一份逻辑 payload，设备参与时的总体效率”

如果后续想更强调“总搬运字节数”，可以再单独增加一个 `transport_bytes` 口径。

## 3. CPU 占用怎么来的

当前实现使用：

- `clock_gettime(CLOCK_MONOTONIC)`：获取 wall time
- `getrusage(RUSAGE_SELF)`：获取进程 user/system CPU 时间

因此输出的：

- `cpu_user_pct`
- `cpu_sys_pct`

反映的是**当前 bench 进程**在这段时间窗口里的 CPU 占用占比。

## 4. 建议的报表字段

CSV 至少建议保留：

- `mode`
- `payload_bytes`
- `iterations`
- `warmup`
- `success_ops`
- `failed_ops`
- `success_rate`
- `avg_us`
- `p50_us`
- `p95_us`
- `p99_us`
- `throughput_mbps`
- `cpu_user_pct`
- `cpu_sys_pct`
- `notes`
