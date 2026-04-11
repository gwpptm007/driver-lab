# third_party

Day27 不直接内置 `pciutils` 源码，以免把整个项目包撑大。

请在 `day27` 目录内按下面命令准备：

```bash
mkdir -p third_party
git clone https://github.com/pciutils/pciutils.git third_party/pciutils
chmod +x third_party/pciutils/lib/configure
chmod +x third_party/pciutils/configure 2>/dev/null || true
make build-lspci
```
