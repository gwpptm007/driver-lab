# patch_trace_mapping

| 主题 | 对应到哪里 | 证据类型 |
|---|---|---|
| patch 点 | `virtio_net` 中的具体位置 | patch / code path |
| before/after 差异 | stats / ip link / workload | diff |
| queue/poll 现象 | runtime / trace | trace/log |
| 评审结论 | review note / final report | 文档 |

## 使用建议

这份表最适合在评审和分享时使用，帮助说明：
- 为什么改这里
- 改完后看到了什么
- 哪些证据来自哪里
