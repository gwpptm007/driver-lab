# 03_PROBE_TX_RX_READING_ORDER

## 第一轮：从 probe/remove 入手

先不要急着抠 TX/RX 细节，先把骨架建立起来。

### 建议顺序

1. 找 `virtio_driver` 注册点
2. 找 `probe()` / `remove()`
3. 看 feature negotiation 在哪里做
4. 看 netdev 是怎么 alloc / init / register 的
5. 看 queue / virtqueue / napi 是怎么初始化的
6. 看 `remove()` 如何逆向释放和清理

## 建议输出一个骨架流程

```text
virtio driver register
  -> probe
    -> negotiate features
    -> alloc/init net_device
    -> init queue / virtqueue / napi
    -> register_netdev
  -> open/close
  -> remove
```

## 第二轮：进入 TX/RX

### 先看 TX
只回答这几个问题：
- `ndo_start_xmit` 从哪里开始？
- skb 何时塞入 virtqueue？
- 什么时候 kick/notify？
- completion/reclaim 何时回来？

### 再看 RX
只回答这几个问题：
- receive buffer 何时预投递？
- 收包后如何唤醒 poll？
- poll 中如何构建 skb？
- GRO / checksum / XDP 入口在哪里？
