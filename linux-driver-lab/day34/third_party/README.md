# day34 third_party 说明

- `pciutils/`：用于构建 guest 侧静态 `lspci`
- 获取方式：

```bash
cd day34
make fetch-pciutils
make build-lspci
```

如果你已经在 day29~day33 构建过 `lspci`，也可以直接在 `env/local_wq7.env` 中把 `GUEST_LSPCI_BIN` 指到已有产物。
