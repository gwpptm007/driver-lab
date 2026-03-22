# START_HERE

你现在只需要按这一条线跑，不要在 day22 目录里到处找文档。

## 第一步：载入本地环境

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day22
source env/local.wq7.env
```

如果你不是当前这台机器，先复制模板：

```bash
cp env/local.example.env env/local.wq7.env
vi env/local.wq7.env
source env/local.wq7.env
```

## 第二步：按固定顺序执行

```bash
make check
make build-tools
make selftest-tool
sudo -E make rootfs
sudo chown -R "$USER:$USER" workdir
make backend
make run
```

`make module` 不是 day22 核心验收硬门槛。当前它更适合作为 day23 的前置问题单独处理。

## 第三步：判定 day22 是否通过

先看：

```bash
cat records/${RUN_ID}/run-summary.md
```

再看：

```bash
sed -n '1,120p' records/${RUN_ID}/lspci-nn.txt
sed -n '1,220p' records/${RUN_ID}/lspci-vv-nn.txt
grep 'DAY22' records/${RUN_ID}/serial.log
```

**注意：当前 `run-summary.md` 可能误判。**

最终请以 `serial.log` 中的 marker 和 `lspci` 实际输出为准。详细标准见：

- `docs/02_RESULTS_AND_ACCEPTANCE.md`
