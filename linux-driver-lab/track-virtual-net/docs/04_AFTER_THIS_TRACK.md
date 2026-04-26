# 04_AFTER_THIS_TRACK

## 做完本 Track 之后

最自然的下一条线是：

```
track-dpdk/
```

原因：
- 你已经有 kernel netdev / virtio / tap / vhost 基础
- 后续 DPDK 可以自然接到：
  - vmxnet3 testpmd
  - virtio-user / vhost-user
  - userspace fastpath
  - AF_XDP / KNI / tap 对照

## 推荐后续方向

```
track-dpdk/
├── lab-vmxnet3-testpmd/
├── lab-virtio-user-vhost/
├── lab-vhost-user-basic/
└── project-user-space-fastpath/
```