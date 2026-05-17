# 02_ACCEPTANCE

## 最低通过

```text
lab-bpftrace-netdev-observe 能跑通环境检查和至少一个 bpftrace 观测脚本。
```

## 标准通过

```text
完成 RX/TX/NAPI/softirq 的 bpftrace records，并能生成 review bundle。
```

## 项目通过

```text
完成 libbpf net observer，并能把事件输出整理成可读报告。
```

## 不夸大边界

这条 track 的早期 lab 是观测实验，不等价于完整产品。只有 `project-linux-network-observability` 完成后，才适合描述为工具型项目。
