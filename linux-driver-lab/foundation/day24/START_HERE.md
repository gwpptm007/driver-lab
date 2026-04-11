# START_HERE

按下面顺序执行，不要跳步骤。

## 1. 进入 day24 并载入环境

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day24
source env/local.wq7.env
```

## 2. 准备当前 day 自己的 pciutils 源码与 arm64 静态 lspci

```bash
mkdir -p third_party
# 能联网时：
git clone https://github.com/pciutils/pciutils.git third_party/pciutils
# 不能联网时：把其它机器上的 pciutils 源码离线拷到 third_party/pciutils

chmod +x third_party/pciutils/lib/configure
chmod +x third_party/pciutils/configure 2>/dev/null || true

make build-lspci
file third_party/pciutils/lspci
```

## 3. 执行 day24 主流程

```bash
make check
make kernel-module-tree
make build-tools
make module
sudo -E make rootfs
sudo chown -R "$USER:$USER" workdir
make backend
sudo -E make run
```

## 4. 看结果

```bash
cat records/${RUN_ID}/run-summary.md
```

详细解释与通过标准见 `docs/02_RESULTS_AND_ACCEPTANCE.md`。构建或运行异常先看 `docs/03_TROUBLESHOOTING.md`。
