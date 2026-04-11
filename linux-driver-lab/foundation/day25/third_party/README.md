# day24 第三方源码说明

为避免把 `pciutils` 源码和构建产物直接打进项目，`day24` 默认**不再内置**
`third_party/pciutils/`。

如果你要在本地跑通 `day24`，请先按照：

- `docs/02_PREPARE_LSPCI.md`

完成下面这件事：

1. 获取 `pciutils` 源码
2. 放到 `day24/third_party/pciutils/`
3. 交叉编译出 arm64 静态 `lspci`
4. 让 `GUEST_LSPCI_BIN` 指向：

```bash
$PWD/third_party/pciutils/lspci
```

完成后再执行：

```bash
make check
make build-tools
sudo -E make rootfs
```
