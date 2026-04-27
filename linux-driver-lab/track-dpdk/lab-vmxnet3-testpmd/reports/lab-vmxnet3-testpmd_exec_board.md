# lab-vmxnet3-testpmd_exec_board

| Phase | 项目 | 状态 | 证据 |
|------|------|------|------|
| P0 | 测试机环境对齐 | 待测试机确认 | docs/04_TEST_MACHINE_ENV.md |
| P1 | 只读环境检查 | DONE | records/*/ENV_CHECK.txt |
| P2 | hugepage 配置 | DONE | records/*/HUGEPAGE_SETUP.txt |
| P3 | vmxnet3 bind | DONE | records/*/BIND_AFTER.txt |
| P4 | testpmd smoke | DONE | records/*/TESTPMD.log |
| P5 | stats 收集 | DONE | records/*/BIND_STATUS.txt |
| P6 | review bundle | DONE | records/*/REVIEW_BUNDLE.md |

## 默认测试机

- Guest: Ubuntu 22.04.5 Desktop / Linux 6.8.0-110-generic
- 管理口: ens33 / e1000 / 192.168.65.135
- DPDK口: ens192 / vmxnet3 / 0000:0b:00.0
