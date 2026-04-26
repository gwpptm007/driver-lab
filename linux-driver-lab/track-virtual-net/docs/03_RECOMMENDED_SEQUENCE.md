# 03_RECOMMENDED_SEQUENCE

## 推荐顺序

### 第一步：`lab-virtio-tap-bridge-path`
先把最基础路径跑通。

### 第二步：`lab-virtio-vhost-kick-notify`
在基础路径稳定后，把 backend 从 userspace 视角扩到 vhost。

### 第三步：`lab-two-guest-bridge-flow`
把单 guest 扩到双 guest。

### 第四步：`project-virtual-net-end-to-end`
收成完整项目。

## 为什么这个顺序最稳

因为它符合从简单到复杂的路径：

```text
single guest + tap + bridge
  -> vhost acceleration
  -> two guest topology
  -> project summary
```

不要一上来同时处理 guest、host、vhost、bridge、two guest、DPDK。