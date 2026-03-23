# day34 常用命令

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day34
chmod +x scripts/*.sh
authorize=$(true)
chmod +x guest/init.day34
source env/day34.env
source env/local_wq7.env 2>/dev/null || true

make check
make fetch-pciutils
make build-lspci
make kernel-module-tree
make build-tools
make module
sudo -E make rootfs
make backend
sudo -E make run
```
