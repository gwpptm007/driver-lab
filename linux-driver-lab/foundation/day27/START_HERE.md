# Day27 Start Here

## 1. 先准备本地环境文件

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day27
source env/local.wq7.env
```

## 2. 再补执行位

```bash
chmod +x scripts/*.sh
chmod +x guest/init.day27
chmod +x third_party/pciutils/lib/configure
chmod +x third_party/pciutils/configure 2>/dev/null || true
```

## 3. 最短执行顺序

```bash
make build-lspci
make check
make kernel-module-tree
make build-tools
make module
sudo -E make rootfs
sudo chown -R "$USER:$USER" workdir
make backend
sudo -E make run
```

## 4. 最先看哪里

```bash
cat records/${RUN_ID}/run-summary.md
sed -n '1,80p' records/${RUN_ID}/lspci-nn.txt
cat records/${RUN_ID}/loop-summary.txt
sed -n '1,120p' records/${RUN_ID}/dmesg-driver.txt
```

## 5. 当前这批测试结果怎么看

如果你看到：
- `lspci-nn.txt` 中有 `1234:11e8`
- `loop-summary.txt` 中 `pass=200`、`fail=0`
- `serial.log` 中有 `===DAY27:COMPLETE===`
- `dmesg-driver.txt` 中大量出现 `probe success`、`irq handler`、`remove leave`

就可以判定 Day27 通过。
