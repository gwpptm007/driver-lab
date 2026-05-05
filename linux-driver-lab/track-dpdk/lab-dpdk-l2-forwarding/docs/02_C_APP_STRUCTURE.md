# 02_C_APP_STRUCTURE

## 文件

```text
app/main.c        l2fwd-lite 主程序
app/meson.build   Meson 构建文件
app/Makefile      make 包装入口
```

## 关键数据结构

```c
struct app_config
```

保存运行参数：

```text
mbuf 数量
burst size
rx/tx descriptor 数量
run seconds
stats period
promisc
```

```c
struct port_sw_stats
```

保存软件统计：

```text
rx_packets
rx_bytes
tx_packets
tx_bytes
tx_failed
no_peer_drop
```

## 端口配对

程序按枚举顺序配对：

```text
port_ids[0] <-> port_ids[1]
port_ids[2] <-> port_ids[3]
```

如果端口数量为 1 或奇数，最后一个端口没有 peer，收到包后释放 mbuf 并增加 `no_peer_drop`。

## 为什么要交换 MAC

最小 L2 forwarding 不做 MAC 学习，只演示二层头部处理能力：

```text
eth.dst <-> eth.src
```

这一步对应真实数据面中的 L2 rewrite 思路。

## 与后续 fastpath 的关系

后续项目会在此基础上继续扩展：

```text
parse Ethernet
parse IPv4
parse UDP
UDP-only fast path
rewrite MAC/IP/UDP
per-flow stats
control-plane config
```
