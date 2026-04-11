# Day26 Quick Commands

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day26
source env/local.wq7.env

mkdir -p third_party
# git clone https://github.com/pciutils/pciutils.git third_party/pciutils

# 新包解压后建议先补执行位
chmod +x scripts/*.sh
chmod +x guest/init.day26
chmod +x third_party/pciutils/lib/configure
chmod +x third_party/pciutils/configure 2>/dev/null || true

make build-lspci
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
