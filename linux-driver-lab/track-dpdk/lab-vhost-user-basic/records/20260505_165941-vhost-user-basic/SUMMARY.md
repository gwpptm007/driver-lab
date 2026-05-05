# SUMMARY

## Lab

lab-vhost-user-basic

## 测试机环境

- Guest：Ubuntu 22.04.5 Desktop
- Kernel：Linux 6.8.0-110-generic
- 管理网卡：ens33 / e1000 / 192.168.65.135
- vhost-user socket：/tmp/dpdk-vhost-user0
- DPDK 版本：21.11.9

## 目标

- [x] 只读环境检查
- [x] hugepage 配置（1024 × 2MB = 2GB）
- [x] testpmd 启动 vhost-user backend
- [x] UNIX domain socket 创建
- [x] port/stats 输出
- [x] 不操作物理网卡 bind/unbind

## 执行命令

```bash
./scripts/00_check_env.sh
sudo ./scripts/01_setup_hugepages.sh
sudo ./scripts/02_run_vhost_testpmd.sh
./scripts/03_collect_stats.sh
./scripts/04_make_review_bundle.sh
```

## 关键结果

| 项目 | 结果 |
|------|------|
| EAL IOVA 模式 | PA（物理地址） |
| mbuf pool | mb_pool_0, n=155456, size=2176 |
| vhost socket | socket_ready=1, /tmp/dpdk-vhost-user0 |
| Port 0 MAC | 56:48:4F:53:54:00 |
| RX/TX packets | 0（无 virtio peer） |

## 问题

- 无实际问题

## 下一步

- Phase 3: lab-virtio-user-vhost
- 将 virtio-user frontend 与 vhost-user backend 对接
