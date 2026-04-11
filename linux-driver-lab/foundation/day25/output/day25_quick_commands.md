# Day25 最短命令清单

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day25
source env/local.wq7.env

mkdir -p third_party

git clone https://github.com/pciutils/pciutils.git third_party/pciutils
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
cat records/${RUN_ID}/irq-count-before.txt
cat records/${RUN_ID}/irq-count-after.txt
cat records/${RUN_ID}/proc-interrupts-before.txt
cat records/${RUN_ID}/proc-interrupts-after.txt
sed -n '1,220p' records/${RUN_ID}/dmesg-driver.txt
grep -n 'DAY25\|probe\|irq handler\|remove' records/${RUN_ID}/serial.log
```
