# START_HERE

当前进入 eBPF 网络可观测性主线。

## 推荐阅读顺序

```text
1. docs/fundamentals/README.md
2. docs/fundamentals/00_15_MINUTE_MENTAL_MODEL.md
3. docs/fundamentals/01_EBPF_KERNEL_ARCHITECTURE.md
4. docs/fundamentals/02_PROGRAM_TYPES_AND_HOOK_SELECTION.md
5. docs/fundamentals/visuals/README.md（00-02 的 GIF/PNG/Canvas 视觉伴读）
6. docs/fundamentals/03_VERIFIER_MEMORY_AND_SAFETY.md
7. docs/fundamentals/04_MAPS_STATE_AND_CONCURRENCY.md
8. docs/fundamentals/08_BTF_CORE_LIBBPF_AND_SKELETON.md
9. docs/fundamentals/10_NETWORK_PATH_CORRELATION.md
10. docs/fundamentals/12_DEBUGGING_PROJECT_MAP_AND_RECALL.md
11. README.md / ROADMAP.md
12. lab-bpftrace-netdev-observe/START_HERE.md
```

知识层审计 marker：`EBPF_OBSERVABILITY_FUNDAMENTALS_COMPLETE`。

```bash
bash tests/check_fundamentals.sh
```

## 第一站

```text
lab-bpftrace-netdev-observe
```

完成 fundamentals 后，这一站先不写复杂 C/libbpf 程序，而是用 bpftrace 验证刚建立的网络路径观测模型：

```text
netif_receive_skb
napi_poll
dev_queue_xmit
irq:softirq_entry / irq:softirq_exit
```
