# Day20 脚本架构设计

## 1. 总体思路

Day20 最推荐的结构是“两层脚本”：

- 宿主机侧脚本
- guest 侧脚本

这样做的好处是：

- 宿主机负责 QEMU 生命周期与结果收集
- guest 负责内核态/用户态验证动作
- 问题定位更清楚

---

## 2. 宿主机侧脚本职责

建议未来主入口叫：

- `run_day20_regression.sh`

它负责：

1. 检查 `Image`、`rootfs.img`、`demo_regmap.ko`、`perf` 等输入是否存在  
2. 启动 QEMU  
3. 捕获串口输出  
4. 等待 shell prompt  
5. 触发 guest 侧 smoke/trace/perf 脚本  
6. 收集 guest 输出和返回码  
7. 归档到 `records/<timestamp>-day20-.../`  
8. 生成 pass/fail summary  

---

## 3. guest 侧脚本职责

建议拆成几类：

- `guest_day20_smoke.sh`
- `guest_day20_trace.sh`
- `guest_day20_perf.sh`

### `guest_day20_smoke.sh`

负责：

- 挂载 `/proc /sys /dev /debugfs`
- `insmod /demo_regmap.ko`
- 检查 snapshot / trigger
- `rmmod`
- 输出检查结果与返回码

### `guest_day20_trace.sh`

负责：

- 检查 `available_tracers`
- 启动 function_graph 或 Day13 相关 trace
- 归档 trace 文本

### `guest_day20_perf.sh`

负责：

- 检查 `perf version`
- 运行 `perf list`
- 运行 `perf stat true`
- 归档 perf 输出

---

## 4. 结果目录建议

建议仍然沿用 Day17 / Day18 的 records 风格，例如：

```text
records/
└── 20260315-xxxxxx-day20-smoke-arm64-virt/
    ├── serial.log
    ├── guest_smoke.log
    ├── guest_trace.log
    ├── guest_perf.log
    ├── dmesg_tail.txt
    ├── mount.txt
    ├── available_tracers.txt
    ├── perf_stat.txt
    ├── pass_fail.env
    └── summary.txt
```

这样做的优点是：

- 和 Day17 / Day18 的 records 风格统一
- 后续 Day21 引用材料也容易

---

## 5. pass / fail 判定建议

不要只根据单个命令返回码来定成功。
建议拆成分项：

- `BOOT_OK`
- `DEBUGFS_OK`
- `DEMO_INSMOD_OK`
- `SNAPSHOT_OK`
- `TRIGGER_OK`
- `RMMOD_OK`
- `FGRAPH_OK`
- `PERF_OK`
- `DMESG_CLEAN`

最终再给一个总判定：

- `REGRESSION_PASS=1` 或 `0`

这样失败时能直接看是哪一类项挂了。

---

## 6. 与 Day06 / Day17 / Day18 的关系

### 复用 Day06 的思想

- 装卸回归
- 压力脚本
- dmesg 扫描

### 复用 Day17 / Day18 的骨架

- run_qemu
- profile collect
- records 归档
- 指标和日志文件布局

所以 Day20 不是从零开始，而是把前面已经验证过的脚手架重新拼成自动回归链。
