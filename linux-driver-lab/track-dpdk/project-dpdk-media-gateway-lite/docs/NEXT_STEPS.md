# NEXT_STEPS

## 当前状态

```
PASS_SMOKE
```

project-dpdk-media-gateway-lite 已完成：
- 代码骨架（EAL / mempool / ethdev / RX-TX loop）
- vdev null pair smoke 测试可启动
- 双端口转发配置可打印
- per-port / per-rule / drop reason 统计
- UDP-only drop 路径可验证

## 下一步目标

```
PASS_TRAFFIC
PASS_FORWARDING
PASS_REWRITE
```

## 推荐测试路径

| 路径 | 依赖 | 复杂度 | 推荐度 |
|------|------|--------|--------|
| C: pcap PMD | 无 | 低 | **最稳，先做** |
| B: virtio-user txonly | 无 | 中 | 其次 |
| A: 真实 vmxnet3 + 外部发包 | 外部发包源 | 高 | 最后 |

### 路径 A：真实 vmxnet3 + 外部 UDP 发包
```
外部 VM/宿主机 -> ens192/vmxnet3 -> media-gateway-lite
```

### 路径 B：vhost-user / virtio-user txonly
```
testpmd txonly -> virtio-user -> vhost socket -> media-gateway-lite
```

### 路径 C：pcap PMD（最稳）
```
预生成 UDP pcap -> net_pcap rx -> media-gateway-lite -> net_null tx
```

## 待修复问题

| 编号 | 问题 | 优先级 |
|------|------|--------|
| 1 | main.c tx_burst 后 mbuf 访问问题 | P0 |
| 2 | parse_gateway_stats.py 累计统计问题 | P1 |
| 3 | net_null 脚本文档注释缺失 | P2 |

## 待新增内容

| 编号 | 内容 |
|------|------|
| 4 | 真实 UDP 流量测试脚本 |
| 5 | rewrite 命中测试 |

## 结论

project-dpdk-media-gateway-lite 已从"代码骨架"升级为：
> "可启动、可跑双端口、可统计、可验证 UDP-only drop 路径"的项目

下一步：补真实 UDP traffic / forwarding / rewrite 验证

## 范围边界

- 当前不做 KNI
- 当前不做 v17 review