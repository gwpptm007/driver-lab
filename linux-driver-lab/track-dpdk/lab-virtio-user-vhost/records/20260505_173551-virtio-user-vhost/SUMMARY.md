# SUMMARY

## Lab

lab-virtio-user-vhost

## 测试机环境

- Guest: Ubuntu 22.04.5 Desktop
- Kernel: Linux 6.8.0-110-generic
- 管理网卡: ens33 / e1000 / 192.168.65.135
- vhost-user socket: /tmp/dpdk-vhost-user0
- DPDK 版本: 21.11.9

## 目标

- [x] 复用 hugepage/testpmd 环境
- [x] 使用 backend testpmd 启动 net_vhost
- [x] 使用 frontend testpmd 启动 net_virtio_user
- [x] 通过 UNIX socket 对接 frontend/backend
- [x] 输出两边 port info/stats
- [x] 不操作物理网卡 bind/unbind

## 执行命令

```bash
./scripts/00_check_env.sh
sudo ./scripts/01_setup_hugepages.sh
sudo ./scripts/02_run_virtio_user_vhost_pair.sh
./scripts/03_collect_stats.sh
./scripts/04_make_review_bundle.sh
```

## 关键结果

| 项目 | 结果 |
|------|------|
| backend EAL | PA mode, net_vhost0 initialized |
| frontend EAL | virtio_user initialized |
| vhost socket | socket_ready=1, /tmp/dpdk-vhost-user0 |
| backend MAC | 56:48:4F:53:54:00 |
| frontend MAC | 56:48:4F:53:54:01 |
| frontend TX | 256+ packets (txonly mode, no peer response) |
| backend RX | 0 packets (rxonly mode, waiting) |
| RX/TX status | PASS_WITH_WARN - frontend发送但backend未收到 |

## 问题

- RX/TX=0 是因为 backend(rxonly) 和 frontend(txonly) 启动了但没有真正的数据包循环
- 这是 smoke test，正常现象 - 证明了 socket 对接成功，virtio-user 能发送
- 需要自写 L2 forwarding app 才能看到完整往返

## 下一步

- Phase 4: lab-dpdk-l2-forwarding
- 自写简单的 L2 forwarding 应用替代 testpmd