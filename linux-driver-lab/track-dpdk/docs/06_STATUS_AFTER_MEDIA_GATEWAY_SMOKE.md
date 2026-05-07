# 06_STATUS_AFTER_MEDIA_GATEWAY_SMOKE

## 当前决策

`project-dpdk-media-gateway-lite` 当前不继续补真实流量，先进入：

```text
project-dpdk-v17-legacy-review
```

## media-gateway-lite 当前状态

```text
PASS_SMOKE
PASS_UDP_ONLY_DROP_PATH
```

已证明：

```text
EAL 初始化
双 vdev port
主 loop
软件 stats
rte_eth_stats
udp_only=1 下 non-UDP drop path
```

未证明：

```text
PASS_TRAFFIC
PASS_FORWARDING
PASS_REWRITE
```

## 为什么继续 v17 review

这一步可以先把 DPDK track 的业务价值收束起来：

```text
旧 DPDK v17 媒体面经验
  -> 当前 modern DPDK 实验链
  -> media-gateway-lite 项目型骨架
  -> 面试表达
  -> 简历素材
```

真实流量闭环后面再回补，不影响当前做经验复盘。
