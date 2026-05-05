# REVIEW_BUNDLE

## Lab

lab-virtio-user-vhost

## Review conclusion

| Item | Status | Evidence |
|------|--------|----------|
| backend/frontend commands generated | PASS | TESTPMD_COMMANDS.txt |
| vhost-user socket created | PASS | VHOST_SOCKET.txt |
| backend net_vhost log available | PASS | TESTPMD_BACKEND.log |
| frontend virtio-user log available | PASS | TESTPMD_FRONTEND.log |
| backend stats command executed | PASS | TESTPMD_BACKEND.log |
| frontend stats command executed | PASS | TESTPMD_FRONTEND.log |
| packet counter non-zero | PASS_NONZERO_PACKET_COUNTER | TESTPMD_BACKEND.log / TESTPMD_FRONTEND.log |
| fatal/error quick scan | CHECK_LOG | TESTPMD_BACKEND.log / TESTPMD_FRONTEND.log |
| physical NIC untouched | PASS_BY_DESIGN | 两个 testpmd 均使用 --no-pci |

## Acceptance summary

本实验验证 DPDK 本机虚拟链路：

1. backend `net_vhost` 创建 `/tmp/dpdk-vhost-user0`。
2. frontend `net_virtio_user` 使用同一路径连接 backend。
3. 两边 testpmd 均输出 port info/stats。
4. 不操作真实物理网卡。

如果 packet counter 非零，说明 smoke test 进一步产生了可观察收发证据；如果为零但 backend/frontend/socket/stats 均 PASS，可以按 `PASS_WITH_WARN` 收口，再进入下一站自写 L2 app。

## Key files

- `TESTPMD_COMMANDS.txt`
- `TESTPMD_BACKEND.log`
- `TESTPMD_FRONTEND.log`
- `VHOST_SOCKET.txt`
- `RUNTIME_STATUS.txt`
- `POST_CHECK.txt`

## Next

进入：

`track-dpdk/lab-dpdk-l2-forwarding`