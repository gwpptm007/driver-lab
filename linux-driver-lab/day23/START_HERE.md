# day23 START_HERE

## 先记结论

day23 的目标已经验证通过：`pci_driver` 骨架可以成功接住 `ivshmem 1af4:1110`，并完成 `probe/remove + BAR` 资源识别。

## 你现在只需要按这一条流程跑

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
```

## 跑完以后只看这几个文件

```bash
cat records/${RUN_ID}/run-summary.md
sed -n '1,200p' records/${RUN_ID}/dmesg-probe.txt
sed -n '1,160p' records/${RUN_ID}/dmesg-remove.txt
grep -n 'DAY23\|probe\|remove\|BAR0\|BAR2\|ivshmem' records/${RUN_ID}/serial.log
```

## day23 通过标准

- `driver/day23_ivshmem_probe.ko` 成功生成
- `insmod` 成功
- `probe enter` 出现
- `BAR0:`、`BAR2:` 出现
- `probe success` 出现
- `rmmod` 成功
- `remove enter`、`remove leave` 出现
- `===DAY23:COMPLETE===` 出现

## 今天不要抢跑的内容

- 不做共享内存协议
- 不做 ioctl/read/write
- 不做 MSI
- 不做 mmap

这些留给 day24/day25/day26。
