# project-virtual-net-end-to-end

> 所属：`track-virtual-net/`

## 一句话定位

这是 `track-virtual-net` 的阶段收尾项目：

> **把 `lab-virtio-tap-bridge-path`、`lab-virtio-vhost-kick-notify`、`lab-two-guest-bridge-flow` 三个实验收成一个完整的虚拟化网络端到端项目。**

## 它要整合什么

### Lab1：tap/bridge 基础路径

```text
guest virtio_net
  -> QEMU tap backend
  -> tap-vnet0
  -> br-vnet0
  -> host IP stack
```

### Lab2：vhost/kick/notify

```text
vhost=off:
  QEMU userspace backend

vhost=on:
  host kernel vhost_net backend
```

### Lab3：two guest bridge flow

```text
guest A
  -> tap A
  -> bridge
  -> tap B
  -> guest B
```

## 项目交付目标

1. 一张完整拓扑图
2. 一套 QEMU 网络参数
3. 一份 tap/bridge 基础路径记录
4. 一份 vhost=off/on 对照记录
5. 一份 two guest flow 记录
6. 一份 final report
7. 一份 share script
8. 一份下一步接 DPDK 的计划

## 推荐阅读顺序

1. `START_HERE.md`
2. `docs/01_PROJECT_GOAL.md`
3. `docs/02_INPUT_LABS.md`
4. `docs/03_FINAL_TOPOLOGY.md`
5. `docs/04_EVIDENCE_COLLECTION.md`
6. `docs/05_FINAL_REPORT_TEMPLATE.md`
7. `docs/06_SHARE_SCRIPT.md`
8. `docs/07_NEXT_TRACK_DPDK.md`
9. `reports/final_project_report.md`
