# day31 详细计划

## 1. 今日主题

bench：吞吐 / 延迟 / CPU 占用

## 2. 核心目标

基于 day30 已经跑通的 coherent DMA + `mmap` 主链路，完成一版最小 bench，输出：

- `ioctl` 控制路径基线
- `mmap` 用户态路径
- `DMA` 端到端路径
- 延迟分位数
- 吞吐
- CPU 占用

## 3. 今日最小闭环

### 输入

- QEMU EDU 设备
- arm64 guest 工具链
- day31 独立驱动与 guest bench 工具

### 过程

1. 加载 `day31_edu_bench.ko`
2. 完成一次 `mmap-verify`，确认功能基线仍然成立
3. 依次执行 `bench-ioctl / bench-mmap / bench-dma`
4. 运行 `bench-all`，产出多负载结果
5. 归档 serial、bench 输出、driver dmesg、run summary

### 输出

- `records/<RUN_ID>/bench-ioctl.txt`
- `records/<RUN_ID>/bench-mmap.txt`
- `records/<RUN_ID>/bench-dma.txt`
- `records/<RUN_ID>/bench-all.txt`
- `records/<RUN_ID>/run-summary.md`
- `output/day31_bench_report_template.csv`

## 4. Day31 三条路径的真实含义

### 4.1 ioctl path

- 被测动作：`DAY31_IOC_GET_INFO`
- 用途：提供“单次控制路径”的最轻基线
- 价值：帮助判断 DMA 路径里，纯 syscall / ioctl 成本占了多大比例

### 4.2 mmap path

- 被测动作：用户态在映射区执行 `fill + clear + memcpy + memcmp`
- 用途：观察用户态直接访问 coherent buffer 的成本
- 价值：和 DMA 路径对比时，可以看出“设备参与”本身的开销

### 4.3 dma path

- 被测动作：用户态填 src、清 dst，驱动执行两段 DMA，然后用户态 compare
- 用途：观察最接近真实设备参与的数据路径的表现
- 价值：Day31 的主角路径

## 5. 建议统计口径

- `warmup = 20`
- `iterations = 200`
- `payload_bytes = 64 / 256 / 1024 / 2048`

输出至少包含：

- `min / avg / p50 / p95 / p99 / max`
- `throughput_mbps`
- `cpu_user_pct / cpu_sys_pct`
- `success_rate`

## 6. 今日结束前自查

- `mmap-verify` 是否仍能通过
- 至少一条 `bench-all` 结果是否已经生成
- `records/` 是否包含 raw serial 与 raw bench 输出
- 文档是否写清楚统计口径和限制条件
