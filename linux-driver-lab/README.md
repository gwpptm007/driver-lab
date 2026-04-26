# linux-driver-lab

Linux 驱动学习实验主目录。从 foundation（day01~day35）到 netdev（stage00~stage14），再到 track 专题研究，形成完整的驱动学习路径。

## 目录结构

```
linux-driver-lab/
├── docs/           项目文档（专家评审、进度、架构）
├── foundation/      ⭐ 第一阶段：day01~day35（W1~W5）
├── netdev/         ⭐ 第二阶段：stage00~stage14
├── track-real-driver/   第三阶段：真实驱动源码专题
├── track-virtual-net/   虚拟化网络：tap/bridge/vhost
├── track-af-xdp/        AF_XDP 快速路径
├── track-dpdk/          DPDK 用户态网络
└── track-ebpf-observability/  eBPF 可观测性
```

## 快速导航

| 阶段 | 入口 | 说明 |
|------|------|------|
| foundation | `foundation/README.md` | W1~W5 完整学习路径 |
| netdev | `netdev/README.md` | stage00~stage14 网络驱动主线 |
| track-real-driver | `track-real-driver/lab-virtio-net-source-dive/README.md` | 真实驱动源码深潜 |
| track-virtual-net | `track-virtual-net/README.md` | vhost/kick/notify 机制 |

详细说明 → `docs/01_PROGRAMS.md`