# day24 本地执行手册

## 1. 前置说明

`driver-lab/kernel-src` 不在当前代码包里，所以 day24 只把内核/BusyBox/交叉工具链当作你本机已有前置，day24 自身目录内不再依赖其它 day 的产物。

你需要准备：

- arm64 内核源码树与 `build/arm64`
- arm64 `Image`
- arm64 BusyBox
- `aarch64-linux-gnu-` 交叉工具链
- 当前 day24 自己目录下的 `pciutils` 源码

## 2. 主流程总览

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day24
source env/local.wq7.env

mkdir -p third_party
# 能联网时：
git clone https://github.com/pciutils/pciutils.git third_party/pciutils
# 不能联网时：把其它机器上的 pciutils 源码离线拷到 third_party/pciutils

chmod +x third_party/pciutils/lib/configure
chmod +x third_party/pciutils/configure 2>/dev/null || true

make build-lspci
file third_party/pciutils/lspci

make check
make kernel-module-tree
make build-tools
make module
sudo -E make rootfs
sudo chown -R "$USER:$USER" workdir
make backend
sudo -E make run
```

## 3. day24 之前必须确认的前置条件

### 3.1 PCI 内核配置已经打开

至少应为：

- `CONFIG_PCI=y`
- `CONFIG_PCI_MSI=y`
- `CONFIG_PCI_HOST_GENERIC=y`
- `CONFIG_MODULES=y`
- `CONFIG_MODULE_UNLOAD=y`

### 3.2 模块树已同步到当前配置

如果之前打开 PCI 之后没有重新生成模块树，`make module` 很容易在 `modpost` 阶段报 PCI 符号 undefined。

推荐直接执行：

```bash
cd /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src

export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-
unset CC CXX LD AR AS NM STRIP OBJCOPY OBJDUMP READELF

make -C /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src   ARCH=arm64   CROSS_COMPILE=aarch64-linux-gnu-   O=/home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/build/arm64   modules_prepare

make -C /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src   -j"$(nproc)"   ARCH=arm64   CROSS_COMPILE=aarch64-linux-gnu-   O=/home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/build/arm64   modules
```

## 4. 运行后如何判断通过

请直接看：

```bash
cat records/${RUN_ID}/run-summary.md
sed -n '1,200p' records/${RUN_ID}/mmio-info.txt
sed -n '1,200p' records/${RUN_ID}/mmio-read-before.txt
sed -n '1,200p' records/${RUN_ID}/mmio-write-state.txt
sed -n '1,200p' records/${RUN_ID}/mmio-read-after.txt
sed -n '1,200p' records/${RUN_ID}/shm-write.txt
sed -n '1,200p' records/${RUN_ID}/shm-read.txt
sed -n '1,240p' records/${RUN_ID}/dmesg-driver.txt
```

这些输出怎么解释，见 `docs/02_RESULTS_AND_ACCEPTANCE.md`。
