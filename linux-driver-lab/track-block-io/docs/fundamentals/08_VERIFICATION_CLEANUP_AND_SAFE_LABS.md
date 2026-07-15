# 08. 验证、清理与安全实验

block driver 实验可能破坏文件系统或让宿主机 I/O 卡住，因此“能加载”不是验收。安全边界、清理顺序与记录和读写功能同等重要。

## 8.1 实验前的硬规则

- 只操作新建的教学 block device，绝不把 mkfs、dd 或 fio 写到宿主系统盘；
- 每条命令先确认目标路径、major/minor、容量和 mountpoint；
- 使用独立 VM 或可恢复环境；保留 console/快照/重启方案；
- 不在已 mount、仍被进程打开或仍有 I/O 的设备上卸载模块；
- 所有破坏性命令都使用显式设备名和小容量/短时 workload。

## 8.2 分层验收

| 层次 | 验收 | 失败时先查 |
| --- | --- | --- |
| 注册 | module load、设备节点、lsblk/capacity | init 资源与 gendisk/queue 顺序 |
| raw I/O | pattern write/read、边界检查 | sector/length、segment、方向 |
| filesystem | mkfs、mount、创建/读取文件、umount | capacity、flush/错误、metadata 路径 |
| workload | 小 fio smoke、无错误、记录输出 | 参数、page cache、queue/in-flight |
| 清理 | umount、无 open handle、rmmod、设备消失 | in-flight、quiesce、资源释放 |

每层通过后再进入下一层。文件系统损坏或卸载卡住时，先保存 dmesg/trace/状态，避免反复强制操作覆盖证据。

## 8.3 记录应包含什么

每次实验记录模块版本、目标 kernel/config、设备容量、命令、完整 stdout/stderr、dmesg 前后片段、lsblk/mount 状态、fio job、统计计数和清理结果。性能测试还要补 CPU/NUMA 与 direct/buffered 信息。

## 8.4 回归与故障注入

除正常 read/write 外，至少验证：模块参数非法、内存分配失败模拟、越界 sector、未支持 opcode、重复 load/unload、卸载期间拒绝新 I/O。故障注入的目标是证明错误能被传播和资源能被清理，而非追求更多日志。

当前 Phase 1 的验收 marker 仍以 README 定义的 PASS_REGISTER、PASS_READ_WRITE、PASS_MKFS、PASS_MOUNT、PASS_FIO_SMOKE、PASS_CLEANUP 为准。
