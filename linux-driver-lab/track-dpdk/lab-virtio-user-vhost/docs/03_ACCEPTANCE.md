# 03_ACCEPTANCE

## PASS 标准

| 检查项 | 通过标准 | 证据 |
|---|---|---|
| testpmd 可发现 | `ENV_CHECK.txt` 能找到 `dpdk-testpmd` 或 `testpmd` | `ENV_CHECK.txt` |
| hugepage 可用 | `HugePages_Total` 大于 0 | `HUGEPAGE_SETUP.txt` |
| backend 启动 | 日志中有 `net_vhost` / `vhost` / `Port` / `testpmd>` | `TESTPMD_BACKEND.log` |
| socket 创建 | `socket_ready=1` | `VHOST_SOCKET.txt` |
| frontend 启动 | 日志中有 `virtio_user` / `net_virtio_user` / `Port` / `testpmd>` | `TESTPMD_FRONTEND.log` |
| stats 命令执行 | 两边日志中有 port stats 相关输出 | `TESTPMD_BACKEND.log` / `TESTPMD_FRONTEND.log` |
| 不影响物理 NIC | 两条命令都存在 `--no-pci` | `TESTPMD_COMMANDS.txt` |

## 可接受的 WARN

| WARN | 是否影响通过 | 说明 |
|---|---|---|
| RX/TX 为 0 | 默认不影响 | 本阶段优先验证 frontend/backend 对接；非零包计数作为增强证据 |
| dmesg 无权限 | 不影响 | 普通用户执行收集可能出现 |
| socket 退出后不存在 | 不影响 | testpmd 退出后 socket 被清理属于正常现象 |
| frontend 日志关键字因 DPDK 版本不同略有差异 | 需人工复核 | 看 `Port 0`、stats、没有明显 fatal error 即可 |

## FAIL 标准

- backend testpmd 无法启动。
- `socket_ready=0`。
- frontend testpmd 无法启动或无法识别 `net_virtio_user0`。
- 两边日志完全没有 port/stats 输出。
- 误操作真实网卡导致管理口断开。
