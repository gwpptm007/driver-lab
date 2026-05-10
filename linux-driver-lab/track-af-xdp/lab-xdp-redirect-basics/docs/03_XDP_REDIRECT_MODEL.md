# 03_XDP_REDIRECT_MODEL

## XDP action

XDP 程序最终返回一个 action：

```text
XDP_PASS      放行到内核协议栈
XDP_DROP      在 XDP 层丢弃
XDP_REDIRECT  重定向到 map 指定的目标
```

本 lab 通过 `config_map[0]` 控制 action。

## stats map

程序使用 per-CPU array 统计不同 action 的包数和字节数：

```text
stats_map[action].packets
stats_map[action].bytes
```

用户态 loader 周期性读取并汇总每个 CPU 的值。

## XSKMAP 与 AF_XDP 的关系

AF_XDP socket 创建后，会把 socket fd 写入 XSKMAP：

```text
queue_id -> xsk fd
```

XDP 程序执行：

```c
bpf_redirect_map(&xsks_map, ctx->rx_queue_index, XDP_PASS)
```

然后包才会进入对应 AF_XDP socket 的 RX ring。

## 为什么本 lab 只做 redirect dry-run

本 lab 还没有创建 AF_XDP socket，也没有把 socket fd 写入 XSKMAP。

所以 `XDP_REDIRECT` 在这里只能证明：

```text
BPF 程序具备 XSKMAP redirect 代码路径
loader 可以设置 action=redirect
```

不能证明完整 AF_XDP 收包成功。完整验证放到下一站。
