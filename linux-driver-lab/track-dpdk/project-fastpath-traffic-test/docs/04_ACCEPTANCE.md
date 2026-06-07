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

**结论**: ✅ PASS_SMOKE 已验证 (2026-05-07, vmxnet3)

## pcap PMD 测试（推荐首选验证 PASS_TRAFFIC/FORWARDING/REWRITE）

```bash
# PASS_TRAFFIC + PASS_FORWARDING
./scripts/06_run_pcap_rx_test.sh

# PASS_REWRITE
REWRITE_ENABLE=1 ./scripts/06_run_pcap_rx_test.sh
```

pcap PMD 测试拓扑：

```text
net_pcap0 (UDP pcap, infinite replay)
    -> fastpath-lite (classify + MAC swap + optional rewrite)
    -> net_null0 (TX accept + discard)
```

预期结果：
- port 0: rx>0, ipv4>0, udp>0 (PASS_TRAFFIC)
- port 1: tx>0 (PASS_FORWARDING)
- rewrite>0 when --rewrite 1 (PASS_REWRITE)

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

## 实测记录

### 2026-06-07 pcap PMD (traffic test)

| 检查项 | 状态 |
|--------|------|
| fastpath-lite 启动 | ✅ |
| port 0/1 started | ✅ |
| enter fastpath loop | ✅ |
| rx>0, ipv4>0, udp>0 | ✅ (rx=111709760) |
| tx>0 | ✅ (tx=111709760) |
| bye | ✅ |

**结论**: ✅ PASS_SMOKE + PASS_TRAFFIC + PASS_FORWARDING

### 2026-06-07 pcap PMD (rewrite test)

| 检查项 | 状态 |
|--------|------|
| rewrite_enable=1 | ✅ |
| rewrite>0 | ✅ (rewrite=77210432) |
| dst_mac/dst_ip/dst_port 配置 | ✅ |

**结论**: ✅ PASS_SMOKE + PASS_TRAFFIC + PASS_FORWARDING + PASS_REWRITE

## 不能误判的情况

如果 `rx=0/tx=0`，即使程序运行成功，也只能是：

```text
PASS_SMOKE
```

不能写成 `PASS_TRAFFIC` 或 `PASS_FORWARDING`。
