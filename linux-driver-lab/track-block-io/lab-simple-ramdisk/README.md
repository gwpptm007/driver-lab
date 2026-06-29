# lab-simple-ramdisk

> track-block-io Phase 1：最小教学型 ramdisk block driver。

## 目标

写一个内存后端的 block device，把 block layer 的最小闭环跑通：

```text
insmod
  -> register block device
  -> /dev/labram0
  -> dd read/write
  -> mkfs.ext4
  -> mount
  -> file I/O
  -> fio smoke
  -> umount/rmmod cleanup
```

## 计划文件

- `docs/01_LAB_OVERVIEW.md`
- `../docs/01_BLOCK_LAYER_MODEL.md`
- `../docs/02_IMPLEMENTATION_PLAN.md`
- `../docs/03_ACCEPTANCE.md`

## 当前状态

```text
PLANNED
```

本目录当前只放规划入口，后续实现时再补 `driver/`、`scripts/`、`records/`、`reports/`。

