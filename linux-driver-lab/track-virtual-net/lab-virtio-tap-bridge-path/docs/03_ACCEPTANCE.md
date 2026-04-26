# 04_ACCEPTANCE

## 最低通过

- host 有 `br-vnet0`
- host 有 `tap-vnet0`
- tap 已加入 bridge
- guest 能看到 virtio-net 网卡
- guest 能 ping host bridge IP

## 标准通过

- 有 tcpdump 记录
- 有 QEMU 参数记录
- 有 host/guest IP 记录
- 有 `SUMMARY.md`

## 优秀通过

- 能画出 guest -> tap -> bridge -> host 的完整路径
- 能说明它和前面 `virtio_net` driver 视角的关系
