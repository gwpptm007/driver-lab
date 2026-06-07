# START_HERE

当前目标：整合 Phase 1-4 → 统一网络可观测性项目。

## 阅读顺序

```text
1. README.md              — 项目概览
2. src/net_observer.h     — 事件类型与数据结构
3. src/net_observer.bpf.c — BPF 内核程序
4. src/net_observer.c     — Userspace 加载器 + 报告生成
5. Makefile               — 构建流程
```

## 操作顺序

```bash
# 1. 编译
bash scripts/01_build.sh

# 2. 运行 (需 root)
sudo build/net_observer -v -d 15

# 3. 生成报告
bash scripts/03_generate_report.sh
```

## 关键特性

- per-interface RX/TX/DROP/GRO 统计
- per-CPU 事件分布
- DROP 原因分类 (从 kfree_skb reason 字段)
- 路径分析 (RX→GRO, TX-QUEUE→TX-XMIT)
- Markdown 结构化报告
