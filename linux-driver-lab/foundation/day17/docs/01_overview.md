# 01_overview - Day17 的目标和边界

Day17 的目标不是继续“新增一个驱动知识点”，而是把 W3 当前最重要的工程化主线收口成一套独立目录：

- baseline
- trim
- rootfs
- tracing
- perf（可选注入）
- guest/host 自动采样
- 结果归档

它的边界也很清楚：

- **day17 管流程和实验目录**
- **kernel-src 管源码和编译产物**

也就是说：

- 内核源码还在 `kernel-src/linux-5.15.10/src`
- 内核输出目录还在 `kernel-src/linux-5.15.10/build/arm64`
- BusyBox 源和安装输出还在 `kernel-src/busybox-1.36.1/...`
- 但“怎么用这些东西跑完整实验”，统一写在 day17 里
