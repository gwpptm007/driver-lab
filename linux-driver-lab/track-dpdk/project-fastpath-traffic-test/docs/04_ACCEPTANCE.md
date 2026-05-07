# 04_ACCEPTANCE

## PASS_SMOKE

```text
BUILD.log 存在
FASTPATH_RX.log 存在
日志含 port started / enter fastpath loop / stats / bye
```

### 验证记录

| 检查项 | 2026-05-07 |
|--------|------------|
| fastpath-lite 启动 | ✅ |
| port 0 started | ✅ |
| enter fastpath loop | ✅ |
| fastpath-lite software stats | ✅ |
| bye | ✅ |

**结论**: ✅ PASS_SMOKE 已验证

## PASS_TRAFFIC

```text
PASS_SMOKE
rx > 0
ipv4 > 0 或 udp > 0
COMPARE_STATS.txt 能解析出非零计数
```

## PASS_REWRITE

```text
PASS_TRAFFIC
rewrite_enable=1
rewrite > 0
```

## PASS_FORWARDING

```text
双端口或 vhost/virtio-user 拓扑
rx > 0
tx > 0
```

## 不能误判的情况

如果 `rx=0/tx=0`，即使程序运行成功，也只能是：

```text
PASS_SMOKE
```

不能写成 `PASS_TRAFFIC` 或 `PASS_FORWARDING`。
