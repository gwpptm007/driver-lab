# day31 bench 设计说明

## 1. 为什么 Day31 要单独做 bench

到了 day29 / day30，主功能链路其实已经比较明确：

- day29：coherent DMA round-trip
- day30：`mmap` 零拷贝访问 coherent DMA buffer

如果 Day31 还只是“再跑一遍功能验证”，学习收益会迅速下降。真正更有价值的是：

- 这些路径的固定成本是多少
- 小负载和大负载下表现如何
- DMA 参与后比纯 `mmap` 慢多少
- CPU 使用情况有什么变化

## 2. 当前 tool 的三种模式定义

### 2.1 bench-ioctl

单次操作：

- `ioctl(fd, DAY31_IOC_GET_INFO, &info)`

意义：

- 这是 Day31 的控制路径基线
- 它告诉我们：哪怕完全不搬数据，一次用户态到内核态来回也有成本

### 2.2 bench-mmap

单次操作：

1. 在映射区 `src` 写 pattern
2. 清空 `dst`
3. `memcpy(dst, src, len)`
4. `memcmp(src, dst, len)`

意义：

- 这是纯用户态访问映射区的“内存路径”
- 这里没有设备参与，所以可以视为 DMA 路径的对照组

### 2.3 bench-dma

单次操作：

1. 在映射区 `src` 写 pattern
2. 清空 `dst`
3. 发起 `DAY31_IOC_RUN_DMA`
4. 设备完成两段 DMA
5. 用户态回到映射区做 `memcmp`

意义：

- 这是 Day31 的主角路径
- 用来观察“真实设备参与的数据通路”

## 3. 为什么 `bench-dma` 仍然保留用户态 compare

因为 Day31 想保持 day30 的核心思想：

- buffer 的可见性留给用户态
- 内核主要负责 DMA 发起与中断处理
- 数据是否一致由用户态自己判断

这样零拷贝的学习线索才不会被“内核又帮忙做 compare”冲淡。
