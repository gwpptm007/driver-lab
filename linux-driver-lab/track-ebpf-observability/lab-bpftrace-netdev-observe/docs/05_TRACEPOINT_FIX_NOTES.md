# 05_TRACEPOINT_FIX_NOTES

## 背景

上一轮测试中出现：

```text
ERROR: Could not resolve symbol: /proc/self/exe:BEGIN_trigger
```

同时部分 kprobe 在当前测试机上受 BTF/notrace 影响，不适合作为强验收点。

## 修正策略

```text
1. 移除所有 bpftrace BEGIN / END block
2. 主路径改成 tracepoint
3. kprobe 降级为 optional
4. 增加 XDP 清理脚本，避免旧 XDP 程序绕过 skb 路径
5. review bundle 改为 PASS_TRACEPOINT_RX / PASS_TRACEPOINT_TX / PASS_SOFTIRQ
```

## 验收口径

```text
PASS_TRACEPOINT_SMOKE:
  tracepoint 脚本能运行并正常 timeout 退出

PASS_TRAFFIC_OBSERVED:
  tracepoint 输出里出现非 0 count

KPROBE_OPTIONAL:
  可用则记录，不可用不阻塞本 lab
```
