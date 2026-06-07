# lab-libbpf-net-observer

> 从 bpftrace 脚本 → C/libbpf 编译型观测工具

## 目标

把 Phase 1-3 中 bpftrace 验证过的观测点（netif_receive_skb, net_dev_start_xmit, kfree_skb 等）迁移到 C/libbpf + ringbuf，形成可独立部署的网络观测二进制程序。

## 技术栈

| 组件 | 技术 | 阶段 |
|------|------|------|
| BPF 内核代码 | C + vmlinux.h (CO-RE) | `skb_observer.bpf.c` |
| BPF 编译 | clang -target bpf | `build/*.bpf.o` |
| Skeleton 生成 | bpftool gen skeleton | `build/*.skel.h` |
| Userspace 加载器 | C + libbpf | `skb_observer.c` |
| 数据传输 | ringbuf (BPF_MAP_TYPE_RINGBUF) | 实时事件流 |
| 编译 | Makefile (4 步自动化) | `make` |

## 与 Phase 3 的对照

| 维度 | Phase 3 (bpftrace) | Phase 4 (libbpf) |
|------|--------------------|-------------------|
| 代码量 | ~50 行脚本 | ~300 行 C (BPF + userspace) |
| 编译 | 无需 (解释执行) | clang + bpftool + gcc |
| 输出 | terminal printf maps | ringbuf 事件流 |
| 部署 | bpftrace 必须在目标机 | 单一二进制 |
| CO-RE | 不支持 | vmlinux.h + BTF |
| 性能 | 编译开销每次运行 | 编译一次，直接执行 |

## 目录

```text
src/
  skb_observer.h        # 共享定义 (事件类型、结构)
  skb_observer.bpf.c    # BPF 内核程序 (tracepoint handlers)
  skb_observer.c        # Userspace 加载器 (skeleton + ringbuf poll)
Makefile                # 自动化编译 (4 步)
scripts/
  00_check_env.sh       # 环境检查 (clang, bpftool, libbpf, BTF)
  01_build.sh           # 编译
  02_run_observer.sh    # 运行观测
  03_make_review_bundle.sh  # 自动判卷
  08_traffic_hint.sh    # 流量提示
  09_clean_runtime.sh   # 清理
records/
reports/
```

## 快速运行

```bash
cd lab-libbpf-net-observer

# Step 1: 环境检查
sudo bash scripts/00_check_env.sh

# Step 2: 编译
bash scripts/01_build.sh

# Step 3: 运行 (另开窗口制造流量: ping -i 0.1 <网关>)
sudo EBPF_DURATION=10 bash scripts/02_run_observer.sh

# Step 4: 判卷
bash scripts/03_make_review_bundle.sh
```

## 当前结论

待测试。

## 下一站

本 lab 完成后进入 Phase 5：`project-linux-network-observability`，形成完整的网络路径观测工具。
