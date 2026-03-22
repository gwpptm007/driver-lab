# day23 quick commands（最终版）

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day23
source env/local.wq7.env

make check
make kernel-module-tree
make module
sudo -E make rootfs
sudo chown -R "$USER:$USER" workdir
make backend
sudo -E make run

cat records/${RUN_ID}/run-summary.md
sed -n '1,200p' records/${RUN_ID}/dmesg-probe.txt
sed -n '1,160p' records/${RUN_ID}/dmesg-remove.txt
grep -n 'DAY23\|probe\|remove\|BAR0\|BAR2\|ivshmem' records/${RUN_ID}/serial.log
cat records/${RUN_ID}/qemu.stderr.log
```
