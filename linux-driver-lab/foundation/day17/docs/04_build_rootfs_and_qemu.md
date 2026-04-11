# 04_build_rootfs_and_qemu - rootfs / DTB / QEMU 组装

## 1. build.sh 做什么

`build.sh` 会完成：

1. 编译 `demo_regmap.ko`
2. 生成 Day17 自己的 `rootfs/`
3. 打包成 `rootfs.img`
4. dump 出 QEMU `virt-base.dtb`
5. 注入 `demo_regmap.fragment.dtsi`
6. 生成 `virt-day17.dtb`
7. 默认直接启动 guest

## 2. build.sh 最常用命令

```bash
./build.sh
```

如果你只想构建，不想马上启动：

```bash
QEMU_AUTO_BOOT=no ./build.sh
```

然后再单独启动：

```bash
./run_qemu.sh
```
