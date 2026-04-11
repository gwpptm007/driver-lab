# day31 third_party

这里预留给 day31 的第三方依赖，主要是 guest 侧静态 `lspci` 所需的 `pciutils`。

## 1. 从 GitHub 获取 pciutils

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day31
source env/day31.env
source env/local_wq7.env   # 或你自己的 local.<name>.env

chmod +x scripts/*.sh
bash scripts/01_fetch_pciutils.sh
```

默认会从下面这个仓库克隆：

```text
https://github.com/pciutils/pciutils.git
```

如需改镜像地址，可在执行前覆盖：

```bash
export PCIUTILS_GIT_URL=<your-mirror-url>
```

## 2. 构建 guest 侧 arm64 静态 lspci

```bash
bash scripts/02_build_guest_lspci.sh
```

脚本内部会：

- 进入 `third_party/pciutils/`
- 补 `configure` 可执行权限
- 使用 `aarch64-linux-gnu-gcc` 做静态构建
- 生成 `third_party/pciutils/lspci`

## 3. 可以直接复用旧的 lspci

若本机已经在 day27/day29/day30 构建过可用的 arm64 `lspci`，也可以直接通过：

```bash
export GUEST_LSPCI_BIN=/path/to/existing/lspci
```

来复用，不必重复构建。
