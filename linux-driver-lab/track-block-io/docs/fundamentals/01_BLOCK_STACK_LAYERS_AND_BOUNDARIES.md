# 01. Block stack 分层与边界

文件 I/O 不是从应用直接跳到驱动。每层解决不同问题，调试和性能归因时必须分清。

| 层 | 主要职责 | 不应据此推断 |
| --- | --- | --- |
| userspace | 文件 API、direct/buffered I/O、fio workload | 一次系统调用就是一次设备请求 |
| VFS/filesystem | 路径、inode、权限、日志、块映射 | 文件写返回就已持久化 |
| page cache/writeback | 缓存、脏页、回写聚合 | 设备吞吐等于应用 write 吞吐 |
| block layer | bio、request、合并、调度、队列与完成 | request 一定与一个 bio 对应 |
| driver | 将 request 映射到后端/硬件、报告状态 | driver 收到请求即完成 |
| device/media | 执行命令、缓存、持久化、错误 | 设备接受 write 等于掉电安全 |

## buffered 与 direct I/O

buffered I/O 可先修改 page cache，随后由 writeback 在不同时间提交 block I/O；direct I/O 试图避开 page cache，但仍受对齐、文件系统和设备限制。两者的 fio 延迟、CPU 和设备统计不可直接混为一谈。

## block device 与文件系统

gendisk/major/minor 向系统呈现块设备，例如 /dev/labram0。mkfs 与 mount 把文件系统建立在该设备之上。教学测试必须先区分 raw block 写与 mounted filesystem 写：后者会引入 metadata、journal、cache 和 writeback。

## 当下 API 边界

现代 Linux 的多队列 block 层以 blk-mq 为中心。规划中提到的简单 queue 只能作为概念阶段；真正实现前必须固定目标 kernel，并按该 kernel 支持的接口实现。不要从旧教程复制已经不适用于目标内核的 legacy request queue 写法。

下一篇：[02 bio、sector 与 segment](02_BIO_SECTORS_SEGMENTS_AND_LIFETIME.md)。
