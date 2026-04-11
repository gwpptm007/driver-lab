# Day34 排障

## 1. `run-result.txt` 里 `mmap_ok=0` / `mmap_error=-22`，是不是整轮失败？

不是。

Day34 的 guest 流程在最后会执行两条错误注入：
1. `fault-invalid-len`
2. `fault-mmap-offset`

而 `result` 读取的是驱动内“最近一次操作结果”快照。当前 records 中最后一次操作是 `fault-mmap-offset 1`，所以 `run-result.txt` 看到的是：
- `mmap_ok=0`
- `mmap_error=-22`
- `mmap_len=4096`
- `mmap_pgoff=1`

这恰恰说明驱动把非法页偏移拒绝下来了，而不是说明整轮主链路失败。

## 2. 为什么并发阶段要对 `stress-mmap` 加 `flock()`？

因为所有 worker 共用同一个 DMA coherent buffer。若每个进程都同时写同一块 `src/dst` 半页，就会把共享缓冲区本身写乱，进而把数据竞争误判成驱动不稳定。

Day34 要验证的是：
- 多进程是否能并发竞争同一设备入口；
- 驱动是否能稳定处理反复的 `RUN_DMA` / `mmap` / `ioctl`。

因此用户态用 `flock()` 将“填充 -> RUN_DMA -> compare”整段串起来，只把竞争集中在设备入口和生命周期，而不是让共享内存本身互相踩踏。

## 3. 为什么 `remove()` 里要手工 `free_irq()` 再 `pci_free_irq_vectors()`？

Day34 会做 1000 次 `insmod/rmmod` 循环。若用 `devm_request_irq()`，devres 释放发生在 `remove()` 返回之后；这会导致 `pci_free_irq_vectors()` 执行时，IRQ 仍然被 IRQ 层引用，从而在 `free_msi_irqs()` 路径上触发 BUG。

因此这里改成：
- `request_irq()`
- `free_irq()`

并在 `remove()` 中明确按：
- `free_irq()`
- `pci_free_irq_vectors()`
- `dma_free_coherent()`

的顺序释放。

## 4. `lspci` 明明复制进 rootfs，guest 里却提示 `not found`

常见原因不是文件没复制进去，而是交叉构建出的 `lspci` 不是静态链接，initramfs 里缺少解释器/动态库。

Day34 已将 `scripts/02_build_guest_lspci.sh` 固定成静态链接构建：
- `CFLAGS=-static`
- `LDFLAGS=-static`
