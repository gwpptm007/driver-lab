# Day32 Perf Summary

- baseline avg_us: 283.590
- optimized avg_us: 0.892
- avg latency gain: 99.69%
- baseline p99_us: 641.344
- optimized p99_us: 0.912
- p99 latency gain: 99.86%
- baseline throughput_mbps: 6.585
- optimized throughput_mbps: 1575.094
- throughput gain: 23819.42%

## Interpretation
- baseline 模式每轮都会重新 GET_INFO + mmap + munmap，热路径里 syscall 和 VMA 建立/销毁占比更高。
- optimized 模式把 layout 查询和 mmap 固定到循环外，减少了重复系统调用与地址空间管理成本。
- Day32 的最小优化点，就是把“每轮重复准备”收敛为“bench 前准备一次，循环内只做核心数据路径”。
