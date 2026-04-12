# 02. virtio-net 阅读地图

建议阅读顺序：

1. `drivers/net/virtio_net.c`
2. `drivers/virtio/virtio_ring.c`
3. `include/uapi/linux/virtio_net.h`

第一口先抓：

- `virtnet_probe`
- `virtnet_open`
- `virtnet_close`
- `virtnet_xmit`
- `virtnet_poll`
- `try_fill_recv`
