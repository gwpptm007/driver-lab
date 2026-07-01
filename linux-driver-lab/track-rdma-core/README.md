# track-rdma-core

这条 track 用来把 RDMA 从“听过很多名词”推进到“能解释、能验证、能落证据”的状态。

当前阶段先不急着写复杂 verbs 程序。第一步是学习 RDMA core 的对象模型，再建立环境边界：测试机有没有 RDMA 设备、有没有 `rdma-core` 工具链、内核有没有 Soft-RoCE 能力、哪些路径只能停留在模型推演。

## 当前进度

| 阶段 | 目录 | 目标 | 状态 |
| --- | --- | --- | --- |
| Phase 0 | `docs/02_RDMA_CORE_MODEL.md` | RDMA core 原理、verbs 对象模型、QP/CQ/MR 学习 | 已补充 |
| Phase 1 | `lab-rdma-env-capability` | RDMA 工具、设备、内核模块、Soft-RoCE 边界采集 | PASS，`rxe0` 已创建 |
| Phase 2 | `lab-rdma-verbs-object-lifecycle` | device/context/PD/MR/CQ/QP 对象生命周期 | PASS |
| Phase 3 | `lab-rdma-memory-region-deep-dive` | MR、lkey/rkey、access flags | PASS |
| Phase 4 | `lab-rdma-qp-state-machine` | RC QP 状态迁移 | PASS |
| Phase 5 | `lab-rdma-rc-pingpong` | RC send/recv 与 CQ completion | PASS |
| Phase 6 | `lab-rdma-one-sided-read-write` | RDMA READ/WRITE one-sided 语义 | PASS |
| Phase 7 | `lab-rdma-ud-rocev2-model` | UD/RoCEv2 报文路径与封装模型 | PASS |
| Phase 8 | `project-rdma-core-summary` | 汇总报告、面试材料、DPDK 到 RDMA 对照 | PASS |

## 学习重点

- RDMA 不是“更快的 socket”，而是把数据路径从 syscall + kernel protocol stack 转为 NIC DMA + userspace queue pair。
- verbs 程序的核心不是 API 背诵，而是对象生命周期：device -> context -> PD -> MR -> CQ -> QP -> WQE -> CQE。
- 没有硬件时也要能诚实收敛：记录缺失点、Soft-RoCE 可行性、哪些结论只能作为模型说明。

## 入口

建议阅读顺序：

```text
docs/01_TRACK_OVERVIEW.md        # RDMA 是什么、解决什么、在系统中的位置
docs/02_RDMA_CORE_MODEL.md       # rdma-core 框架、verbs 对象、MR/QP/CQ
docs/04_DPDK_TO_RDMA_BRIDGE.md   # 从 Linux netdev / DPDK 过渡到 RDMA
ROADMAP.md                       # 后续每个实验阶段怎么推进
START_HERE.md                    # 实际执行命令入口
```

最终总结入口：

```text
project-rdma-core-summary/README.md
```

## 最终结论

测试机 `192.168.65.135` 第一轮采集结果：

- `ibverbs-utils` 已补齐，`ibv_devices` / `ibv_devinfo` 可用。
- Soft-RoCE `rxe0` 已通过 `ens34` 创建。
- `rdma link` 显示 `rxe0/1 state ACTIVE physical_state LINK_UP netdev ens34`。
- Phase 2-7 六个代码项目最终 `make test` 全部通过。
- 已验证 verbs 对象、MR、QP 状态机、RC ping-pong、one-sided READ/WRITE、UD/GRH。
- 总结报告、证据索引、面试笔记、简历材料和 DPDK/RDMA 对比已完成。

证据总入口：

```text
project-rdma-core-summary/EVIDENCE_INDEX.md
```
