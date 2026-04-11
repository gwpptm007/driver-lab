# third_party 说明

day22 最特殊的依赖，是 guest 侧需要一个 **arm64 的 `lspci`**。

因为 guest 常常是 arm64，而宿主机常常是 x86_64，所以不能直接把宿主机 `/usr/bin/lspci` 拷进去。

## 推荐方案

### 方案 A：直接放预编译好的 arm64 静态 `lspci`

你可以把它放到任意路径，然后：

```bash
export GUEST_LSPCI_BIN=/path/to/aarch64-static-lspci
```

### 方案 B：放 pciutils 源码

把源码解压到：

```text
third_party/pciutils/
```

然后执行：

```bash
make build-lspci
```

## 为什么推荐静态 `lspci`

因为 day22 用的是最小 initramfs。
如果 `lspci` 是动态链接，还得再额外处理 guest 里的 libc / libpci / 其他动态库，复杂度会明显上升。
