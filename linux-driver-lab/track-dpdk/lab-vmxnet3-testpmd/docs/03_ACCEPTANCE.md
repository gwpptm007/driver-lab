# 03_ACCEPTANCE

## 最低通过

满足以下条件即可认为本 Lab 第一轮通过：

- `records/<timestamp>-vmxnet3-testpmd/` 已生成
- `00_check_env.sh` 完成并保存 `ENV_CHECK.txt`
- `ens192` 能被识别为 VMXNET3 网卡
- `0000:0b:00.0` 能被 `lspci` 查到
- hugepage 有可用页
- `dpdk-devbind.py --status` 能运行
- `testpmd` 能启动并输出端口信息或 stats

## 标准通过

在最低通过基础上，还需要：

- `ens33` 管理网未被误绑定，SSH 管理链路保持可用
- `0000:0b:00.0` 成功从 `vmxnet3` 切换到 `vfio-pci` 或目标 DPDK driver
- `TESTPMD.log` 中能看到端口初始化、forward mode、stats 输出
- `04_collect_stats.sh` 收集到绑定状态、hugepage、PCI 详情、ip 状态、dmesg 片段
- `05_make_review_bundle.sh` 生成 `REVIEW_BUNDLE.md`
- `reports/lab-vmxnet3-testpmd_exec_board.md` 更新为本轮状态

## 优秀通过

在标准通过基础上，还能解释：

- 为什么 DPDK 使用 hugepage
- 为什么 `ens192` bind 到 `vfio-pci` 后不再出现在普通 `ip addr` 数据面里
- kernel driver `vmxnet3` 与 DPDK PMD 的职责差异
- `testpmd` 的价值：先验证 PMD/队列/mbuf/port，而不是直接写业务 app
- 为什么下一步自然进入 `lab-vhost-user-basic`

## 失败但可接受的记录

如果测试机暂时缺少 DPDK 包、IOMMU 或 vfio 权限，也不算无效。  
只要 records 中清楚保存以下信息，就可以进入评审：

```text
缺少哪个命令
哪个模块加载失败
dpdk-devbind.py 输出了什么
testpmd 报错是什么
HugePages_Total/Free 当前是多少
```

这类失败记录对后续修环境非常有价值。

## 不通过

以下情况不通过：

- 没有 records
- 误操作 `ens33` 导致 SSH 管理链路断开
- 只写文档，没有实际执行日志
- `testpmd` 报错但没有保存完整输出
- 绑定了错误的 PCI 设备
