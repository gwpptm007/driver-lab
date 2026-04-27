# SUMMARY

## Lab

lab-vmxnet3-testpmd

## 测试机环境

- Guest：
- Kernel：
- 管理网卡：ens33 / e1000 / 192.168.65.135
- DPDK 网卡：ens192 / vmxnet3 / 0000:0b:00.0
- DPDK driver：vfio-pci / uio / 其他：

## 目标

- [ ] 只读环境检查
- [ ] hugepage 配置
- [ ] dpdk-devbind 状态确认
- [ ] ens192 绑定到 DPDK driver
- [ ] testpmd 启动
- [ ] stats/logs 收集

## 执行命令

```bash
./scripts/00_check_env.sh
sudo ./scripts/01_setup_hugepages.sh
./scripts/02_bind_vmxnet3.sh status
sudo DPDK_CONFIRM_BIND=YES ./scripts/02_bind_vmxnet3.sh bind
sudo ./scripts/03_run_testpmd.sh
./scripts/04_collect_stats.sh
./scripts/05_make_review_bundle.sh
```

## 关键结果

- hugepage：
- bind 前 driver：
- bind 后 driver：
- testpmd 是否启动：
- stats 是否输出：

## 问题

-

## 原因分析

-

## 修复方法

-

## 下一步

-
