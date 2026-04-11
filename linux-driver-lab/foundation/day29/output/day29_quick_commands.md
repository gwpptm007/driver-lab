# day29 quick commands

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day29
source env/day29.env

chmod +x scripts/*.sh
chmod +x guest/init.day29
chmod +x third_party/pciutils/lib/configure
chmod +x third_party/pciutils/configure 2>/dev/null || true

make check
# 如果 GUEST_LSPCI_BIN 不存在，再执行：make build-lspci
make kernel-module-tree
make build-tools
make module
sudo -E make rootfs
sudo chown -R "$USER:$USER" workdir
make backend
sudo -E make run

cat records/${RUN_ID}/run-summary.md
sed -n '1,120p' records/${RUN_ID}/dma-verify.txt
sed -n '1,120p' records/${RUN_ID}/verify-result.txt
sed -n '1,160p' records/${RUN_ID}/dmesg-driver.txt
```
