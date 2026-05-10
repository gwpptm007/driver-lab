# 01_GOAL_AND_SCOPE

## 目标

`lab-xdp-redirect-basics` 是 AF_XDP track 的第一站。

它的目标不是马上写 AF_XDP socket，而是先确认：

```text
BPF 程序可编译
XDP 程序可 attach
XDP action 可控制
XDP map 统计可观测
XSKMAP redirect 模型可解释
```

## 范围内

- `XDP_PASS`
- `XDP_DROP`
- `BPF_MAP_TYPE_ARRAY` 配置 action
- `BPF_MAP_TYPE_PERCPU_ARRAY` 统计包数/字节数
- `BPF_MAP_TYPE_XSKMAP` 预留 AF_XDP socket 映射
- libbpf 用户态 loader
- records/reports/review bundle

## 范围外

- 完整 AF_XDP socket 创建
- UMEM fill/completion/rx/tx rings
- zero-copy 判断
- 用户态转发器

这些放到后续：

```text
lab-af-xdp-socket-rings
lab-af-xdp-zero-copy-vs-copy
project-af-xdp-mini-forwarder
```
