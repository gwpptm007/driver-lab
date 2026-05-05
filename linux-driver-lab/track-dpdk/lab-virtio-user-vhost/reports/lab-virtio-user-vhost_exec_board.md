# lab-virtio-user-vhost_exec_board

| Phase | 项目 | 状态 | 证据 |
|------|------|------|------|
| P0 | 环境检查 | 待测试 | records/*/ENV_CHECK.txt |
| P1 | hugepage 确认 | 待测试 | records/*/HUGEPAGE_SETUP.txt |
| P2 | backend net_vhost 启动 | 待测试 | records/*/TESTPMD_BACKEND.log |
| P3 | socket 创建 | 待测试 | records/*/VHOST_SOCKET.txt |
| P4 | frontend net_virtio_user 启动 | 待测试 | records/*/TESTPMD_FRONTEND.log |
| P5 | 两边 port info/stats | 待测试 | records/*/TESTPMD_BACKEND.log / TESTPMD_FRONTEND.log |
| P6 | review bundle | 待测试 | records/*/REVIEW_BUNDLE.md |

## 默认命令

```bash
sudo ./scripts/02_run_virtio_user_vhost_pair.sh
```

## 通过门槛

- `VHOST_SOCKET.txt` 中 `socket_ready=1`
- `TESTPMD_COMMANDS.txt` 同时存在 `net_vhost0` 和 `net_virtio_user0`
- backend/frontend 两个日志都有 port/stats 输出
- 两个 testpmd 命令均使用 `--no-pci`
