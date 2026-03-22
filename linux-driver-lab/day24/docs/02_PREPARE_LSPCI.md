# day24：准备 arm64 静态 lspci

这份文档保留给排障时单独查看，但 day24 主流程已经把“获取源码 + 编译 lspci”直接写进：

- `README.md`
- `START_HERE.md`
- `docs/01_LOCAL_RUNBOOK.md`
- `output/day24_quick_commands.md`

## 1. 获取源码

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day24
mkdir -p third_party
# 能联网时：
git clone https://github.com/pciutils/pciutils.git third_party/pciutils
# 不能联网时：把其它机器上的 pciutils 源码离线拷到 third_party/pciutils
```

## 2. 修执行位

```bash
chmod +x third_party/pciutils/lib/configure
chmod +x third_party/pciutils/configure 2>/dev/null || true
```

## 3. 编译

```bash
make build-lspci
```

等价手工命令：

```bash
make -C third_party/pciutils clean
make -C third_party/pciutils   HOST=aarch64-linux-gnu   CROSS_COMPILE=aarch64-linux-gnu-   DNS=no ZLIB=no SHARED=no   LDFLAGS="-static"   lspci
```

## 4. 验证

```bash
file third_party/pciutils/lspci
```
