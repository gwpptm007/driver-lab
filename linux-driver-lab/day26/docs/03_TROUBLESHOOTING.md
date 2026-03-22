# Day26 常见问题

## 1. zip 解压后脚本无法执行

现象常见为：

- `Permission denied`
- `scripts/xx.sh: not found`（实际是没执行位）

建议每次拿到新包后都先执行：

```bash
chmod +x scripts/*.sh
chmod +x guest/init.day26
chmod +x third_party/pciutils/lib/configure
chmod +x third_party/pciutils/configure 2>/dev/null || true
```

虽然新版主流程尽量使用 `bash scripts/xxx.sh`，但把执行位补齐后，手工单步调试会更稳。

## 2. `build-lspci` 失败

先确认第三方源码目录已经准备好：

```text
linux-driver-lab/day26/third_party/pciutils
```

然后执行：

```bash
chmod +x third_party/pciutils/lib/configure
chmod +x third_party/pciutils/configure 2>/dev/null || true
make build-lspci
```

如果当前机器不能联网，就需要你先把 `pciutils` 源码放到上述目录，再执行 `make build-lspci`。

## 3. `make module` 提示 PCI 导出符号 undefined

先执行：

```bash
make kernel-module-tree
```

这个目标会做两件事：

- `modules_prepare`
- 如果 `Module.symvers` 缺少 PCI 符号，再自动跑 `make modules`

## 4. `make rootfs` 提示 `mknod: Operation not permitted`

请使用：

```bash
sudo -E make rootfs
```

并且建议紧接着执行：

```bash
sudo chown -R "$USER:$USER" workdir
```

这样后续 `make backend`、查看 `workdir/` 文件时不会有权限困扰。

## 5. `/dev/day26_edu0` 没出现

guest `init.day26` 会优先挂载 `devtmpfs`；若仍没出现，会从：

```text
/sys/class/day26_edu/day26_edu0/dev
```

读取 `major:minor` 再手工 `mknod`。

## 6. 不要进入 `day26/driver/` 手工 `make`

正确入口是顶层：

```bash
make kernel-module-tree
make build-tools
make module
```

不要在 `day26/driver/` 目录里直接 `make`。
原因是顶层脚本会统一传入：

- `KDIR`
- `ARCH=arm64`
- `CROSS_COMPILE=aarch64-linux-gnu-`

这样才能和当前项目的 arm64 内核构建树保持一致。
