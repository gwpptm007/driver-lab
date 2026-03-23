# day30 third_party

这里预留给 day30 的第三方依赖，例如 guest 侧静态 `lspci` 所需的 `pciutils`。

若本机已经在 day27/day29 构建过可用的 arm64 `lspci`，也可以直接通过：

```bash
export GUEST_LSPCI_BIN=/path/to/existing/lspci
```

来复用，不必重复构建。
