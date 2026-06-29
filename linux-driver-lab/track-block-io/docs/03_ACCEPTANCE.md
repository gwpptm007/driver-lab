# Block I/O Track Acceptance

## Track-level acceptance

最终收口时，应能证明：

```text
PASS_RAMDISK_BLOCK_DEVICE
PASS_READ_WRITE
PASS_MKFS_MOUNT
PASS_BLK_MQ_MODEL
PASS_REAL_DRIVER_MAPPING
PASS_FIO_OBSERVABILITY
PASS_FINAL_REPORT
```

## Phase 1 acceptance

`lab-simple-ramdisk` 的最小验收：

```text
PASS_REGISTER      block device appears in lsblk
PASS_READ_WRITE    dd write/read path works
PASS_MKFS          mkfs.ext4 succeeds
PASS_MOUNT         mount succeeds
PASS_FILE_IO       file create/read succeeds
PASS_FIO_SMOKE     fio produces throughput/latency output
PASS_CLEANUP       umount/rmmod cleanup succeeds
```

## Evidence requirements

每个 lab 至少保留：

- README。
- docs/01_LAB_OVERVIEW.md。
- scripts 或明确命令记录。
- records/<timestamp>/SUMMARY.md。
- 原始命令输出或关键日志。
- 明确 PASS / BLOCKED / LIMITATION 结论。

## Boundary wording

准确表述：

- 教学型 block driver。
- block layer / blk-mq 学习路径。
- fio smoke / latency baseline。

不要夸大：

- 生产级 storage driver。
- 真实 NVMe 性能优化。
- 完整 filesystem/page cache/writeback 覆盖。

