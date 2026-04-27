# lab-vhost-user-basic_exec_board

| Phase | 项目 | 状态 | 证据 |
|------|------|------|------|
| P0 | 环境检查 | 待测试 | records/*/ENV_CHECK.txt |
| P1 | hugepage 确认 | 待测试 | records/*/HUGEPAGE_SETUP.txt |
| P2 | 启动 vhost-user backend | 待测试 | records/*/TESTPMD_VHOST.log |
| P3 | socket 创建 | 待测试 | records/*/VHOST_SOCKET.txt |
| P4 | port info/stats | 待测试 | records/*/TESTPMD_VHOST.log |
| P5 | review bundle | 待测试 | records/*/REVIEW_BUNDLE.md |

## 默认命令

```bash
sudo ./scripts/02_run_vhost_testpmd.sh
```

## 通过门槛

- `VHOST_SOCKET.txt` 中 `socket_ready=1`
- `TESTPMD_VHOST.log` 中有 port/stats 输出
- `TESTPMD_COMMAND.txt` 中存在 `--no-pci`
