# Day27 本地执行手册

## 1. 目标

Day27 只验证一件事：**重复装卸 200 次是否稳定**。

## 2. 环境准备

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day27
source env/local.wq7.env
```

如果 `third_party/pciutils/` 还没准备：

```bash
mkdir -p third_party
git clone https://github.com/pciutils/pciutils.git third_party/pciutils
```

## 3. 补执行位

zip 解压后执行位可能丢失，所以先统一补：

```bash
chmod +x scripts/*.sh
chmod +x guest/init.day27
chmod +x third_party/pciutils/lib/configure
chmod +x third_party/pciutils/configure 2>/dev/null || true
```

## 4. 构建 guest lspci

```bash
make build-lspci
file third_party/pciutils/lspci
```

期望输出里有：`ARM aarch64`、最好还有 `statically linked`。

## 5. 检查环境与内核模块树

```bash
make check
make kernel-module-tree
```

## 6. 构建工具和模块

```bash
make build-tools
make module
ls -l driver/day27_edu_loop.ko
```

## 7. 构建 rootfs 并运行

```bash
sudo -E make rootfs
sudo chown -R "$USER:$USER" workdir
make backend
sudo -E make run
```

## 8. 验证结果

```bash
cat records/${RUN_ID}/run-summary.md
cat records/${RUN_ID}/loop-summary.txt
sed -n '1,120p' records/${RUN_ID}/lspci-nn.txt
sed -n '1,120p' records/${RUN_ID}/dmesg-driver.txt
```

## 9. 如何根据当前测试结果判通过

当前上传的 `records/day27-local-001/` 可以按下面方式判断：

1. `lspci-nn.txt` 中出现 `00:02.0 ... [1234:11e8]`，说明 EDU 设备枚举成功。
2. `loop-summary.txt` 中 `loop_count=200 / pass=200 / fail=0`，说明 200 轮全部通过，没有任何一轮中途失败。
3. `dmesg-driver.txt` 中反复出现 `probe success`、`irq handler`、`remove leave`，说明每轮都完成了“加载 -> 触发中断 -> 卸载”。
4. `serial.log` 最后出现 `===DAY27:COMPLETE===`，说明 guest 自动流程完整跑完。
5. `qemu.stderr.log` 为空，且日志中没有 `BUG:`、`Oops:`、`Kernel panic`、`hung task`，说明循环过程中没有稳定性问题。
