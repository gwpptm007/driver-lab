# 03_ACCEPTANCE

## PASS 标准

| 检查项 | 通过标准 | 证据 |
|---|---|---|
| testpmd 可发现 | `ENV_CHECK.txt` 里能找到 `dpdk-testpmd` 或 `testpmd` | `ENV_CHECK.txt` |
| hugepage 可用 | `HugePages_Total` 大于 0 | `HUGEPAGE_SETUP.txt` |
| vhost-user socket 创建 | `socket_ready=1` | `VHOST_SOCKET.txt` |
| testpmd 有 vhost/port 输出 | 日志中出现 `net_vhost`、`Port`、`testpmd>` 等关键字 | `TESTPMD_VHOST.log` |
| stats 命令执行 | 日志中有 port stats 相关输出 | `TESTPMD_VHOST.log` |
| 不影响物理 NIC | 命令中存在 `--no-pci`，且没有执行 bind/unbind | `TESTPMD_COMMAND.txt` |

## 可接受的 WARN

| WARN | 是否影响通过 | 说明 |
|---|---|---|
| RX/TX 为 0 | 不影响 | 当前没有 virtio peer |
| dmesg 无权限 | 不影响 | 普通用户执行收集时可能出现 |
| socket 退出后不存在 | 不影响 | testpmd 退出后 socket 被清理属于正常现象，重点看运行期间 `socket_ready=1` |

## FAIL 标准

- `socket_ready=0`
- `TESTPMD_VHOST.log` 里没有任何 vhost/testpmd/port 初始化痕迹
- `dpdk-testpmd` 无法启动，且不是路径配置问题
- 误操作物理网卡，导致管理口断开
