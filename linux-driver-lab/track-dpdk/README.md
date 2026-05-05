# track-dpdk

> DPDK 用户态数据面主线

## 一句话定位

从 vmxnet3/testpmd 起步，逐步进入 vhost-user、virtio-user、自写 L2 forwarding C app，最终收成 user-space fastpath 项目。

## 当前测试机环境

```text
Guest: Ubuntu 22.04.5 Desktop / Linux 6.8.0-110-generic
管理口: ens33 / e1000 / 192.168.65.135 (不动)
DPDK口: ens192 / vmxnet3 / 0000:0b:00.0 / uio_pci_generic
DPDK版本: 21.11.9
```

## 5 个实验阶段

| 序号 | 项目 | 状态 | 核心验证 |
|------|------|------|----------|
| 1 | lab-vmxnet3-testpmd | ✅ | hugepage + vmxnet3 + testpmd |
| 2 | lab-vhost-user-basic | ✅ | vhost-user socket 创建 |
| 3 | lab-virtio-user-vhost | ✅ | virtio-user + vhost-user 对接 |
| 4 | lab-dpdk-l2-forwarding | ✅ | l2fwd-lite C app |
| 5 | project-user-space-fastpath | ✅ | 协议分类 + rewrite |

## 快速入口

```bash
cd <lab-directory>
cat START_HERE.md
./scripts/00_check_env.sh
```

## 文档结构

```
track-dpdk/
├── README.md           # 本文件
├── docs/
│   ├── 01_OVERVIEW.md  # 目标、路线图、快速开始
│   ├── 02_ACCEPTANCE.md # 验收标准
│   ├── 03_DPDK_CORE_CONCEPTS.md
│   ├── 04_KERNEL_VS_DPDK_PATH.md
│   ├── 05_DPDK_V17_TO_MODERN_MAPPING.md
│   └── 00_ENVIRONMENT_PREPARE.md
└── [各 lab 目录]
```

## 下一步演进

进入 `project-user-space-fastpath` 后，可选方向：
- per-flow stats
- control-plane config
- records/replay/report
- flow table / ACL
- multi-lcore scaling