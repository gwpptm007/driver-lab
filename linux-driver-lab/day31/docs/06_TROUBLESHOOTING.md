# day31 排障说明

## 1. `/dev/day31_edu0` 不存在

优先看：

- `serial.log`
- `dmesg-driver.txt`
- `/sys/class/day31_edu/day31_edu0/dev` 是否存在

典型原因：

- 驱动 `probe` 失败
- `pci_alloc_irq_vectors()` 或 `request_irq()` 失败
- 设备节点尚未在 guest 里创建

## 2. `mmap-verify` 失败

先区分是：

- `mmap` 本身失败
- `RUN_DMA` 失败
- compare 失败

可依次看：

- `tool-info.txt`
- `mmap-verify.txt`
- `run-result.txt`
- `dmesg-driver.txt`

## 3. `bench-dma` 失败率高

重点看：

- `run_ok`
- `run_error`
- `irq_delta`
- `last_run_ns`
- `dmesg-driver.txt`

当前最常见的可能原因：

- EDU DMA mask 不匹配
- guest 没有使用放宽后的 `-device edu,dma_mask=0xffffffff`
- QEMU 超时或 guest 未完整执行

## 4. 为什么 `bench-ioctl` 的 payload 字段可能看起来没有意义

因为 `bench-ioctl` 本质上测的是控制路径，和 payload 没有强绑定。当前实现里会把 payload 记录为 `0`，不要把它理解成“搬运了 0 字节的数据通路”。

## 5. 为什么 `bench-mmap` 可能比 `bench-dma` 快很多

这通常是正常现象。因为：

- `bench-mmap` 只是用户态访问映射区 + 内存复制
- `bench-dma` 需要经过 ioctl、设备编程、DMA 完成与中断

Day31 的意义就是把这种差异量化出来，而不是假设它们应当接近。
