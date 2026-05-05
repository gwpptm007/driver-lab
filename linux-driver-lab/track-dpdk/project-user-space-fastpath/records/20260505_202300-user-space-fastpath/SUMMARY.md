# SUMMARY

## Project

project-user-space-fastpath

## 测试机环境

- Guest: Ubuntu 22.04.5 Desktop
- Kernel: Linux 6.8.0-110-generic
- 管理网卡: ens33 / e1000 / 192.168.65.135
- DPDK 网卡: ens192 / vmxnet3 / 0000:0b:00.0
- 默认 DPDK driver: uio_pci_generic
- DPDK 版本: 21.11.9

## 目标

- [x] 编译 fastpath-lite (meson + ninja)
- [x] 验证 EAL/mempool/ethdev 初始化
- [x] 实现 L2 转发 + MAC swap
- [x] 添加 UDP-only 分类功能
- [x] 支持 rewrite 规则配置
- [x] 单端口 smoke 验证
- [x] 输出分类统计 (arp/ipv4/udp/non_udp)

## 执行命令

```bash
./scripts/00_check_env.sh
./scripts/01_build_app.sh
sudo ./scripts/02_prepare_vmxnet3.sh
sudo ./scripts/03_run_fastpath_single_port.sh
./scripts/07_collect_stats.sh
./scripts/08_make_review_bundle.sh
```

## 关键结果

| 项目 | 结果 |
|------|------|
| meson | 0.61.2 |
| ninja | 1.10.1 |
| fastpath-lite binary | 220KB, ELF 64-bit |
| EAL IOVA 模式 | PA |
| policy | promisc=1, udp_only=1, swap_mac=1 |
| Port 0 MAC | 00:0C:29:F8:F6:82 |
| 转发模式 | RX/classify/free smoke (单端口) |
| 软件统计 | arp=0, ipv4=0, udp=0, non_udp=0 |

## 与 l2fwd-lite 的区别

| 特性 | l2fwd-lite | fastpath-lite |
|------|-------------|---------------|
| L2 MAC swap | ✅ | ✅ |
| UDP-only 分类 | ❌ | ✅ |
| rewrite 规则 | ❌ | ✅ |
| 精细统计 | 基础 | arp/ipv4/udp/non_udp/rewrite |

## 问题

- 无实际问题
- 单端口 smoke 正常：只有一个 VMXNET3，无法测试真正转发

## 下一步

- 接入第二个 DPDK 口或 vhost/virtio-user 拓扑
- 验证 PASS_FORWARDING (RX/TX 非零)