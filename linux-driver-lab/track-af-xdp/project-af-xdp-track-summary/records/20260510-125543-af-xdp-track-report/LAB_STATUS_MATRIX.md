# AF_XDP Lab Status Matrix

| 阶段 | 当前状态 | 已完成 | 待补 |
|---|---|---|---|
| `lab-xdp-redirect-basics` | `PASS_BASIC_ATTACH` | BPF build、XDP attach/detach、skb mode | DROP、REDIRECT dry-run、非 0 流量统计 |
| `lab-af-xdp-socket-rings` | `READY_TO_TEST` | 代码和脚本已落地 | 测试机 build、UMEM/XSK/rings 验证 |
| `lab-af-xdp-zero-copy-vs-copy` | `READY_TO_TEST` | 代码和脚本已落地 | copy baseline、native/copy、zero-copy probe |
| `project-af-xdp-mini-forwarder` | `READY_TO_TEST` | drop/reflect 项目骨架已落地 | smoke、traffic、TX reflect 验证 |
| `project-af-xdp-track-summary` | `READY` | 总报告、面试材料、简历素材 | 后续根据测试结果刷新 |

## 不夸大的边界

当前 AF_XDP track 已完成工程落地和基础 XDP attach 证明，但仍有多项测试留在 backlog：

```text
DROP/REDIRECT 动作补测
AF_XDP socket/rings 测试机验证
zero-copy 支持边界探测
mini forwarder 真实 traffic / TX reflect 验证
```
