# Block I/O：知识基础导航

本目录为当前 PARKED_PLANNED 的 block I/O 支线准备启动时可复用的基础文档。它不新增驱动代码，不把教学型 ramdisk 说成磁盘/NVMe 驱动，也不产生性能结论。

| 文档 | 核心问题 |
| --- | --- |
| [00 心智模型](00_15_MINUTE_MENTAL_MODEL.md) | 文件 I/O 如何变为 block I/O？ |
| [01 分层与边界](01_BLOCK_STACK_LAYERS_AND_BOUNDARIES.md) | VFS、文件系统、page cache、block layer 与驱动如何分工？ |
| [02 bio 与数据描述](02_BIO_SECTORS_SEGMENTS_AND_LIFETIME.md) | bio、sector、segment 与内存生命周期是什么？ |
| [03 request 与 blk-mq](03_REQUESTS_BLK_MQ_TAGS_AND_COMPLETION.md) | bio 如何变成 request，软件队列和硬件队列如何协作？ |
| [04 教学 ramdisk](04_TEACHING_RAMDISK_DRIVER_LIFECYCLE.md) | 最小驱动需要哪些资源、初始化和退出顺序？ |
| [05 正确性与持久性](05_FLUSH_FUA_DISCARD_ERRORS_AND_INTEGRITY.md) | flush、FUA、discard、错误和完成语义为何不能跳过？ |
| [06 性能与 NUMA](06_STORAGE_PERFORMANCE_QUEUEING_AND_NUMA.md) | fio 数字如何解释，ramdisk 与真实设备差在哪？ |
| [07 观测与排障](07_OBSERVABILITY_TRACEPOINTS_AND_DEBUGGING.md) | 如何用统计和 trace 沿 I/O 路径定位问题？ |
| [08 验证与安全实验](08_VERIFICATION_CLEANUP_AND_SAFE_LABS.md) | 如何在不伤害宿主机数据的前提下验证模块？ |
| [09 路线与源码映射](09_EXTENSION_ROADMAP_AND_VIRTIO_BLK_MAP.md) | 如何从 ramdisk 演进到 blk-mq、virtio-blk 与证据项目？ |

当前启动入口仍是 [README.md](../../README.md)、[ROADMAP.md](../../ROADMAP.md) 和 [lab-simple-ramdisk](../../lab-simple-ramdisk/README.md)。官方 blk-mq 语义参考 [Linux kernel blk-mq documentation](https://docs.kernel.org/block/blk-mq.html)。
