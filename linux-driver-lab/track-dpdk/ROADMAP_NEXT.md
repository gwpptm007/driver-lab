# ROADMAP_NEXT - track-dpdk 后续推进路线

> 基线状态：`project-user-space-fastpath` 已完成 `PASS_SMOKE`，具备编译、端口初始化、poll loop、基础统计能力；但真实流量、UDP 分类命中、rewrite 命中、双向转发还没有完成闭环。

## 当前状态

| 阶段 | 目录 | 状态 | 说明 |
|---|---|---|---|
| 1 | `lab-vmxnet3-testpmd` | `PASS` | vmxnet3/uio/hugepage/testpmd stats 已跑通 |
| 2 | `lab-vhost-user-basic` | `PASS` | vhost-user backend socket/testpmd 已跑通 |
| 3 | `lab-virtio-user-vhost` | `PASS_WITH_WARN` | virtio-user + vhost-user 本机对接已跑通，按记录保留 warning |
| 4 | `lab-dpdk-l2-forwarding` | `PASS_SMOKE` | l2fwd-lite C app 能编译、启动、打印 stats；未证明真实转发 |
| 5 | `project-user-space-fastpath` | `PASS_SMOKE` | fastpath-lite 能编译、启动、初始化 vmxnet3、打印软件 stats；RX/TX 仍为 0 |
| 6 | `project-fastpath-traffic-test` | `NEXT` | 下一步：补真实流量、UDP-only、rewrite、stats 对照 |
| 7 | `project-dpdk-media-gateway-lite` | `PLANNED` | 流量测试通过后再做：简化媒体网关项目化实现 |
| 8 | `project-dpdk-v17-legacy-review` | `PLANNED` | 最后做：把旧 DPDK v17 项目经验和现代 DPDK track 对齐 |

## 为什么下一步不是直接做 media gateway

`project-dpdk-media-gateway-lite` 需要建立在一个事实之上：当前 fastpath 可以处理真实 UDP 流量，并能用 records 证明分类、过滤、重写和统计都能被触发。

当前已有证据只到：

```text
fastpath-lite 编译成功
EAL 初始化成功
vmxnet3 PMD probe 成功
port 0 started
enter fastpath loop
stats 正常打印
```

还缺：

```text
rx 非 0
ipv4 / udp 非 0
drop_non_udp 可验证
rewrite 非 0
必要时 tx 非 0
```

所以顺序必须是：

```text
project-user-space-fastpath(PASS_SMOKE)
  -> project-fastpath-traffic-test(PASS_TRAFFIC/PASS_FORWARDING)
  -> project-dpdk-media-gateway-lite(PASS_PROJECT)
  -> project-dpdk-v17-legacy-review(作品化/面试化)
```

## 下一步执行原则

1. 不新增复杂数据面功能，先验证已有 fastpath-lite 的真实流量路径。
2. 优先使用现有测试机环境：`ens33` 管理口不动，`ens192/0000:0b:00.0` 作为 DPDK 口。
3. 测试记录必须能区分 `PASS_SMOKE`、`PASS_TRAFFIC`、`PASS_FORWARDING`。
4. 如果当前只有一个 DPDK 物理口，先做外部发包源 RX 测试；双口或 vhost/virtio 接入后再做 TX/forwarding。
