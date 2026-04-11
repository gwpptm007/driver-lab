# day34 稳定性设计说明

## 1. 为什么 day34 不再追求新增功能

到 day33 为止，主链路已经具备：

- coherent DMA buffer
- `mmap`
- `RUN_DMA`
- `GET_INFO / GET_RESULT`

day34 的目标不是扩功能，而是证明这些能力在更复杂的运行条件下依然可靠。

## 2. 并发模型

默认并发模型为：

- 3 个 `stress-mmap` worker
- 1 个 `stress-ioctl` worker

之所以不直接做 “全部都跑 DMA 大迭代”，是为了把 day34 重点放在：

- 多进程并发访问同一字符设备
- `mmap + RUN_DMA` 与 ioctl 控制路径混合存在时是否稳定

## 3. 模块循环模型

day34 默认使用 1000 次 `insmod/rmmod` 循环，这是最直观的生命周期压力来源。

这里的观察点包括：

- 模块退出时资源是否释放完整
- 再次插入时设备节点是否还能重新出现
- 是否出现偶发 busy / remove 失败 / 再插入失败

## 4. 错误注入模型

day34 默认覆盖两种：

- 非法长度：验证驱动是否正确拒绝 `len > max_verify_len`
- 非法页偏移：验证 `mmap` 是否只接受 `pgoff == 0`

这两条都属于最小但很有代表性的错误输入。
