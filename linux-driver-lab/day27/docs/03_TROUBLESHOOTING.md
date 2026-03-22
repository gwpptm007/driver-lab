# Day27 常见问题

## 1. `./configure: Permission denied`
zip 解压后执行位丢失。先执行：

```bash
chmod +x third_party/pciutils/lib/configure
chmod +x third_party/pciutils/configure 2>/dev/null || true
```

## 2. `mknod ... Operation not permitted`
`rootfs` 阶段会创建 `/dev/console` 和 `/dev/null`，通常必须：

```bash
sudo -E make rootfs
```

## 3. `modpost: __pci_register_driver undefined`
说明 `Module.symvers` 还没补齐 PCI 导出符号。先执行：

```bash
make kernel-module-tree
```

该脚本会先 `modules_prepare`；如果发现 `Module.symvers` 里还缺 PCI 符号，会再自动补一轮 `make modules`。

## 4. `/dev/day27_edu0` 没出现
guest 中会先依赖 `devtmpfs` 自动创建设备节点；若仍没出现，会 fallback 到从 `/sys/class/day27_edu/day27_edu0/dev` 读取 major:minor 后再 `mknod`。

## 5. 200 次循环中途失败
先看：

```bash
cat records/${RUN_ID}/loop-summary.txt
sed -n '1,240p' records/${RUN_ID}/dmesg-driver.txt
```

若存在 `BUG:`、`Oops:`、`Kernel panic`、`hung task`，`run-summary.md` 会把 `oops/hung/panic found` 标为 `yes`。

## 6. `run-summary.md` 写着 `loop target met (200): no`，但 `pass=200/fail=0`
这通常是旧版 `scripts/08_extract_records.sh` 解析 `loop-summary.txt` 时没有去掉串口日志中的回车符导致的假阴性。

判断是否真的通过时，以这三项为准：
1. `loop-summary.txt` 中 `pass=200` 且 `fail=0`；
2. `serial.log` 中存在 `===DAY27:COMPLETE===`；
3. `serial.log`/`dmesg-driver.txt` 中没有 `BUG:` / `Oops:` / `Kernel panic` / `hung task`。
