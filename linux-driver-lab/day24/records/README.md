# day24 records

每次执行 `make run` 后，真实证据会归档到：

```text
records/<RUN_ID>/
```

## 当前这轮测试（`day24-local-001`）已经证明了什么

- `run-summary.md`：自动流程关键步骤全部成功
- `mmio-info.txt`：设备号、BAR 信息、协议头初始值正确
- `mmio-read-before.txt`：写前 `state=1`
- `mmio-write-state.txt`：驱动记录了 `state=3` 的写入
- `mmio-read-after.txt`：写后成功读回 `state=3`
- `shm-write.txt`：成功写入 `hello_day24_ivshmem`
- `shm-read.txt`：成功原样读回 `hello_day24_ivshmem`
- `dmesg-driver.txt`：`probe success / MMIO write / payload write` 完整
- `lspci-vv-nn.txt`：BAR0/BAR2 Region 信息与驱动日志一致
- `qemu.stderr.log`：为空

## 验收时最该看的文件

- `run-summary.md`
- `mmio-info.txt`
- `mmio-read-before.txt`
- `mmio-write-state.txt`
- `mmio-read-after.txt`
- `shm-write.txt`
- `shm-read.txt`
- `dmesg-driver.txt`
- `lspci-vv-nn.txt`
- `serial.log`

本目录只保存 day24 自己的运行证据，不依赖其它 day 的记录。
