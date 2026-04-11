# Day27 Quick Commands

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day27
source env/local.wq7.env

chmod +x scripts/*.sh
chmod +x guest/init.day27
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
cat records/${RUN_ID}/loop-summary.txt
sed -n '1,120p' records/${RUN_ID}/lspci-nn.txt
sed -n '1,160p' records/${RUN_ID}/dmesg-driver.txt
```
