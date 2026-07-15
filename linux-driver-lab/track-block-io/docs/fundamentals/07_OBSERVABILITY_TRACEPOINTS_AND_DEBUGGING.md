# 07. 可观测性、tracepoint 与排障

block I/O 排障应沿时间线追踪：应用提交、bio 创建、request 入队、driver 派发、设备完成、上层完成。只看 fio 最终数字无法定位排队、驱动或文件系统造成的延迟。

## 7.1 三层证据

| 层 | 观察什么 |
| --- | --- |
| 应用/文件系统 | fio 延迟分位、错误、direct/buffered、mount 与 filesystem 状态 |
| block layer | bio/request 数、merge、queue depth、dispatch/complete 时间、iostat |
| driver/device | opcode、sector、bytes、in-flight、错误、reset/health 与硬件统计 |

各层计数应能解释差值。例如应用发出的 I/O 与 driver 完成数的差可能是 page cache、合并、in-flight 或错误，但不能是未知。

## 7.2 工具选择

| 工具 | 用途 | 边界 |
| --- | --- | --- |
| fio | 生成可复现 workload 与 latency/throughput 输出 | 需记录所有 job 参数 |
| iostat | 设备级吞吐、队列、util 概览 | 不解释单请求路径 |
| block tracepoints / blktrace | 提交、派发、完成时间线 | trace 本身有开销 |
| bpftrace/eBPF | 按进程、设备、opcode、延迟采样聚合 | attach 点与内核版本有关 |
| perf | CPU、锁、cache、调度热点 | 需与相同 workload 对照 |
| dmesg | driver 初始化、错误与卸载证据 | 不能作为唯一性能证据 |

## 7.3 最小指标

ramdisk/未来驱动应至少导出 submitted、completed、failed、read/write bytes、in-flight、最大 in-flight、unsupported opcode、out-of-range、unload rejection。采样 trace 时携带时间、device、queue、opcode、sector、bytes、status，不记录用户数据。

## 7.4 常见症状

| 症状 | 优先检查 |
| --- | --- |
| fio 高延迟但低 util | page cache、iodepth、锁、CPU 调度或提交路径 |
| 高 iodepth 无吞吐提升 | 后端并发、hctx/tag、锁或内存带宽 |
| 数据读回错误 | sector->byte 换算、segment 遍历、范围检查、方向 |
| 卸载卡住 | mounted device、open handle、in-flight I/O、quiesce 顺序 |
| 统计不闭合 | merge、失败路径、部分完成或计数 owner |

下一篇：[08 安全实验与验收](08_VERIFICATION_CLEANUP_AND_SAFE_LABS.md)。
