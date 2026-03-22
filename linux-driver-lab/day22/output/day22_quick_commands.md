# day22 快速命令

## 固定入口

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day22
source env/local.wq7.env
```

## 标准执行顺序

```bash
make check
make build-tools
make selftest-tool
sudo -E make rootfs
sudo chown -R "$USER:$USER" workdir
make backend
make run
```

## 可选：day23 前置检查

```bash
make module
```

## 最快的验收查看方法

```bash
cat records/${RUN_ID}/run-summary.md
sed -n '1,120p' records/${RUN_ID}/lspci-nn.txt
sed -n '1,220p' records/${RUN_ID}/lspci-vv-nn.txt
grep 'DAY22' records/${RUN_ID}/serial.log
```

## 当前推荐的最终判定口径

不要只看 `run-summary.md`。

最终优先看：
- `1af4:1110`
- `Region 0`
- `Region 2`
- `===DAY22:COMPLETE===`
