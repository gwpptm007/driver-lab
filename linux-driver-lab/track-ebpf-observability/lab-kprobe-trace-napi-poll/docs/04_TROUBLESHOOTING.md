# 04_TROUBLESHOOTING

## bpftrace 不存在

```bash
sudo apt update
sudo apt install -y bpftrace linux-tools-common linux-tools-$(uname -r)
```

## kprobe 不可附加

先看可用列表：

```bash
sudo bpftrace -l 'kprobe:*napi*'
sudo bpftrace -l 'kprobe:*poll*'
```

如果函数不存在或不可附加，记录为 WARN，再看 tracepoint softirq 结果。

## 没有事件

常见原因：

```text
目标网卡无流量
包被 XDP 层提前 drop/redirect
观测接口不是实际收包接口
VMware 虚拟网卡流量太少
```

可以用：

```bash
ip -s link show dev ens192
cat /proc/net/softnet_stat
```

或者在另一台机器上对该接口制造 ping/UDP 流量。

## 不要重新引入 BEGIN/END

上一站已经遇到 `BEGIN_trigger` 兼容问题。本 lab 的 bpftrace 脚本刻意不使用 `BEGIN/END`。
