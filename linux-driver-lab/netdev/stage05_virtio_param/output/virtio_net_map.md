# virtio-net 阅读地图

- virtio_net.c: /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/drivers/net/virtio_net.c
- virtio_ring.c: /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/virtio/virtio_ring.c (no)
- include/uapi/linux/virtio_net.h: /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/include/uapi/linux/virtio_net.h (no)

## 关键入口

### probe

```text
3073:static int virtnet_probe(struct virtio_device *vdev)
```

### open

```text
1558:static int virtnet_open(struct net_device *dev)
1920:		/* virtnet_open() will refill when device is going to up. */
```

### close

```text
1938:static int virtnet_close(struct net_device *dev)
```

### xmit

```text
1680:static netdev_tx_t start_xmit(struct sk_buff *skb, struct net_device *dev)
```

### poll

```text
1525:static int virtnet_poll(struct napi_struct *napi, int budget)
```

### refill

```text
1317:static bool try_fill_recv(struct virtnet_info *vi, struct receive_queue *rq,
1402:		still_empty = !try_fill_recv(vi, rq, GFP_KERNEL);
1439:		if (!try_fill_recv(vi, rq, GFP_ATOMIC))
```

