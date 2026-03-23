# day34 详细计划

## 1. 今日主题

稳定性：并发压测 + 1000 次循环 + 错误注入

## 2. 核心目标

把 day33 的“功能可用”推进到 day34 的“稳定性可回归”。

## 3. 今日最小闭环

- 输入：day33 已跑通的 DMA/mmap/ioctl 能力
- 过程：并发、多次模块循环、错误注入
- 输出：records 原始留证 + output 总结

## 4. 实施步骤展开

### 步骤 1：冒烟与基线确认

先执行一次 `mmap-verify`，确认当前设备和驱动基本功能正常。

### 步骤 2：并发压测

默认启动：

- 3 个 `stress-mmap` worker
- 1 个 `stress-ioctl` worker

目的是验证：

- 多进程同时打开同一个字符设备时，不会导致明显的死锁/崩溃
- `mmap + RUN_DMA` 与纯 ioctl 读取路径可以并发共存

### 步骤 3：1000 次模块循环

循环执行：

- `rmmod day34_edu_stability`
- `insmod /root/day34_edu_stability.ko`

这是 day34 最核心的“生命周期回归”检查项。

### 步骤 4：错误注入

至少覆盖两类：

- 非法长度：`len > max_verify_len`
- 非法 mmap offset：`pgoff != 0`

### 步骤 5：汇总与结论

最终摘要至少要回答：

- 并发是否通过
- 模块循环是否通过
- 哪些错误注入按预期被拒绝
- 是否存在 `panic/oops/hung/DMA mapping error`

## 5. 建议当天保留的证据

- 命令原文
- 并发 worker 输出
- 模块循环摘要
- 错误注入输出
- dmesg 片段
- 最终 run summary
