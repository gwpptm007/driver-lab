# 05. Flush、FUA、discard、错误与完整性

存储驱动不能把所有 opcode 都视为普通 read/write。即使教学后端暂不支持某项功能，也必须明确拒绝或正确声明边界，不能静默成功。

## 5.1 完成状态的层次

| 事件 | 可能表示 | 不必然表示 |
| --- | --- | --- |
| request 被 driver 接受 | 已拥有该 request 的处理责任 | 数据已写入 backing store |
| request completed success | 本驱动承诺的操作已完成 | 真实介质掉电安全 |
| filesystem write 返回 | page cache 或文件系统已接受 | block I/O 已完成或 journal 已提交 |
| flush/FUA 成功 | 后端承诺相应顺序/持久性语义 | 系统所有更高层业务已提交 |

ramdisk 没有物理介质，可将它的持久性模型明确为“模块存活期间内存可见”；不要假装实现了掉电语义。

## 5.2 关键操作

| 操作 | 需要定义的语义 |
| --- | --- |
| read/write | 方向、范围、部分完成、错误传播 |
| flush | 是否支持，若支持究竟确保什么顺序/可见性 |
| FUA | 是否需要在单次写上提供稳定性语义 |
| discard/write zeroes | 是真正回收/清零，还是明确不支持 |
| read-only | 写请求如何拒绝 |

初始教学驱动最安全的做法是只支持已实现且能测试的 opcode；对其他操作返回正确错误，而非返回成功后丢弃请求。

## 5.3 错误不是日志文本

错误需以 block status 完成给上层，并计数记录 opcode、sector、length、reason。典型错误包括：超容量、未支持 opcode、资源不足、卸载中、内部一致性失败。不要仅 dmesg 打印错误后仍将 request 标记成功。

## 5.4 数据完整性测试

最小测试不止 dd 成功，还应包括：

- 用可识别 pattern 写入多个 offset 后读回逐字节比较；
- 覆盖边界 sector 与容量末尾；
- 并发读写的预期一致性模型；
- 失败请求后确认邻近范围未被破坏；
- mkfs/mount/umount 后重新检查元数据可读。

真实设备还会涉及 cache、power loss、integrity profile、media error 和 reset；这些属于后续 virtio/NVMe 方向，不能由 ramdisk 覆盖。
