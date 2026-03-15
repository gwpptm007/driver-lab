# W5 数据流先看懂什么

推荐先画出这条线：

1. 用户态准备数据
2. 驱动分配 coherent DMA buffer
3. 驱动把 DMA 地址告诉设备
4. 设备完成 DMA
5. 驱动校验内容
6. 用户态通过 ioctl/mmap 读取结果
7. bench / perf / ftrace 观测这条路径
