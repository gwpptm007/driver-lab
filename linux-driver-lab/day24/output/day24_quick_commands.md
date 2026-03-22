# day24 quick commands

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

cat records/${RUN_ID}/run-summary.md
```
