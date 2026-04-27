# lab-vmxnet3-testpmd

> 所属：`track-dpdk/`  
> 当前阶段：DPDK Track 第一站，先跑通真实测试机上的 `vmxnet3 + testpmd` 基础闭环。

## 一句话定位

在 VMware Ubuntu22 测试机上，用独立的 VMXNET3 网卡 `ens192 / 0000:0b:00.0` 跑通 DPDK 基础环境：

```text
hugepage → vfio-pci/uio → dpdk-devbind → testpmd → port stats → records/report
```

## 已对齐的测试机环境

来自 `track-dpdk/docs/00_ENVIRONMENT_PREPARE.md`：

| 项目 | 当前值 |
|------|--------|
| Guest OS | Ubuntu 22.04.5 Desktop |
| 内核 | Linux 6.8.0-110-generic |
| 管理网卡 | `ens33` / e1000 / NAT / `192.168.65.135` |
| 备用网卡 | `ens34` / e1000 |
| DPDK 测试网卡 | `ens192` / vmxnet3 / `192.168.100.1/24` |
| DPDK PCI BDF | `0000:0b:00.0` |
| 用途边界 | `ens33` 保持 SSH 管理，`ens192` 才允许绑定到 DPDK 驱动 |

> 注意：脚本不会写入测试机密码。涉及 `sudo` 的步骤请在测试机交互输入密码，避免把密码固化进仓库。

## 推荐阅读顺序

1. `START_HERE.md`
2. `docs/01_GOAL_AND_SCOPE.md`
3. `docs/02_EXECUTION_FLOW.md`
4. `docs/03_ACCEPTANCE.md`
5. `docs/04_TEST_MACHINE_ENV.md`
6. `docs/05_HUGEPAGE_AND_BIND.md`
7. `docs/06_TESTPMD_STATS_REVIEW.md`
8. `reports/lab-vmxnet3-testpmd_report.md`

## 脚本入口

```bash
cd linux-driver-lab/track-dpdk/lab-vmxnet3-testpmd

# 1. 只读环境检查，不改系统
./scripts/00_check_env.sh

# 2. 配置 hugepage
sudo ./scripts/01_setup_hugepages.sh

# 3. 查看绑定状态
./scripts/02_bind_vmxnet3.sh status

# 4. 绑定 ens192/0000:0b:00.0 到 vfio-pci
sudo DPDK_CONFIRM_BIND=YES ./scripts/02_bind_vmxnet3.sh bind

# 5. 运行 testpmd 基础验证
sudo ./scripts/03_run_testpmd.sh

# 6. 收集结果
./scripts/04_collect_stats.sh

# 7. 生成 review bundle
./scripts/05_make_review_bundle.sh
```

## 安全边界

- 默认只操作 `0000:0b:00.0` / `ens192`。
- 不会操作 `ens33`，避免 SSH 管理链路断开。
- `bind` 动作必须显式传入 `DPDK_CONFIRM_BIND=YES`。
- `unbind`/恢复内核驱动也需要显式动作，避免误操作。

## 本 Lab 通过后进入

```text
track-dpdk/lab-vhost-user-basic
```

也就是从“真实 VMXNET3 PMD 基础闭环”推进到“DPDK vhost-user 后端”。
