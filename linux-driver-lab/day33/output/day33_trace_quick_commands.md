# Day33 常用命令速查

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day33
source env/day33.env
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
cat records/day33-local-001/run-summary.md
sed -n '1,160p' records/day33-local-001/trace-window.txt
```
