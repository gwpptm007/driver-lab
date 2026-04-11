# Day26 third_party

Day26 不直接内置第三方源码。

如果需要 arm64 `lspci`，请先把 `pciutils` 源码准备到：

```text
third_party/pciutils
```

然后执行：

```bash
make build-lspci
```
