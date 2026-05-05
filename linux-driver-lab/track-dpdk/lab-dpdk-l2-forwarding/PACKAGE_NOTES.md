# PACKAGE_NOTES - lab-dpdk-l2-forwarding

## 本次新增

- 将原来的占位 `main.c` 替换为完整 `l2fwd-lite`。
- 新增 Meson/Makefile 构建入口。
- 新增环境检查、编译、准备 VMXNET3、单端口运行、双端口运行、vdev null pair、收集、评审脚本。
- 新增 docs/runbook/acceptance/report/exec board。

## 当前验收重点

当前测试机只有一个 DPDK VMXNET3 口，因此默认先验收：

```text
PASS_SMOKE
```

也就是：

```text
编译成功
EAL 初始化成功
mbuf pool 创建成功
port/queue 初始化成功
进入 rx_burst 循环
打印软件 stats / ethdev stats
正常退出
```

不是强制要求 RX/TX 非 0。

## 真正 L2 forwarding 的增强条件

需要至少两端：

```text
两个 DPDK 物理/虚拟端口
或 vhost/virtio-user 拓扑
或外部发包器 + 对端
```

后续 `project-user-space-fastpath` 会把这一站的 app 继续工程化。
