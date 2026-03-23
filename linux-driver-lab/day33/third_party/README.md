# day33 third_party 说明

Day33 仍然复用静态 `lspci` 作为 guest 里的 PCI 枚举工具。

首次使用建议：

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day33
make fetch-pciutils
chmod +x third_party/pciutils/configure 2>/dev/null || true
chmod +x third_party/pciutils/lib/configure
make build-lspci
```
