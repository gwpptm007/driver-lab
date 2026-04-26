# ROADMAP

## 总体顺序

1. `lab-vmxnet3-testpmd`
2. `lab-vhost-user-basic`
3. `lab-virtio-user-vhost`
4. `lab-dpdk-l2-forwarding`
5. `project-user-space-fastpath`



## Phase 1: `lab-vmxnet3-testpmd`

目标：

跑通 hugepage、vfio/uio、device bind、testpmd、port stats。

验收：

- 有 records
- 有 report
- 有 review bundle
- 能讲清和前一阶段的关系



## Phase 2: `lab-vhost-user-basic`

目标：

把 track-virtual-net 的 vhost_net 视角推进到 DPDK vhost-user socket。

验收：

- 有 records
- 有 report
- 有 review bundle
- 能讲清和前一阶段的关系



## Phase 3: `lab-virtio-user-vhost`

目标：

不依赖完整 VM，理解用户态 virtio frontend 与 vhost backend。

验收：

- 有 records
- 有 report
- 有 review bundle
- 能讲清和前一阶段的关系



## Phase 4: `lab-dpdk-l2-forwarding`

目标：

实现最小 EAL/mempool/port/queue/rx_burst/tx_burst L2 forwarding。

验收：

- 有 records
- 有 report
- 有 review bundle
- 能讲清和前一阶段的关系



## Phase 5: `project-user-space-fastpath`

目标：

整合 vmxnet3/testpmd、vhost-user、virtio-user、L2 forwarding 成项目。

验收：

- 有 records
- 有 report
- 有 review bundle
- 能讲清和前一阶段的关系
