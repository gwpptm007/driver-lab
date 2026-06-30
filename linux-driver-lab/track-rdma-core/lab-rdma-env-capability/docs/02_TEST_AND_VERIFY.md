# 02_TEST_AND_VERIFY

## 执行命令

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/lab-rdma-env-capability
bash scripts/00_collect_env.sh
bash scripts/01_collect_rdma_capability.sh
bash scripts/02_try_soft_roce_boundary.sh
bash scripts/04_install_ibverbs_utils.sh
bash scripts/03_generate_summary.sh
```

## 可选 Soft-RoCE 尝试

默认不会创建 rxe 设备。确认要尝试时再执行：

```bash
ENABLE_RXE_SETUP=1 RXE_NETDEV=ens192 bash scripts/02_try_soft_roce_boundary.sh
bash scripts/01_collect_rdma_capability.sh
bash scripts/03_generate_summary.sh
```

## 验证命令

```bash
latest="$(ls -td records/20* | head -1)"
cat "$latest/SUMMARY.md"
grep -E "PASS_|BLOCKED_|WARN_" "$latest/SUMMARY.md"
```

## 需要保留的原始证据

```bash
cat "$latest/ENV_CHECK.log"
cat "$latest/RDMA_CAPABILITY.log"
cat "$latest/SOFT_ROCE_BOUNDARY.log"
cat "$latest/INSTALL_IBVERBS_UTILS.log"
cat "$latest/OPERATION_LOG.md"
```

## 判读重点

- `ibv_devices` 有输出：可以进入真实 verbs 设备实验。
- `ibv_devices` 无设备，但 `rdma_rxe` 存在：可尝试 Soft-RoCE。
- 工具不存在：先安装 `rdma-core` / `ibverbs-utils` 后再继续。
- `rdma_rxe` 不存在：当前内核无法直接做 Soft-RoCE，需要换内核模块或环境。

## 每条命令在学什么

| 命令 | 作用 |
| --- | --- |
| `00_collect_env.sh` | 看 OS、kernel、PCI、网卡、RDMA 模块和工具是否存在 |
| `01_collect_rdma_capability.sh` | 从 `ibv_*` 和 `rdma` 两个视角看 RDMA device |
| `02_try_soft_roce_boundary.sh` | 默认只看 `rdma_rxe` 是否可用，不自动创建 rxe |
| `04_install_ibverbs_utils.sh` | 补齐 `ibv_devices` / `ibv_devinfo`，并记录 dpkg 锁等阻塞 |
| `03_generate_summary.sh` | 把原始日志归纳成 PASS/BLOCKED 状态 |
