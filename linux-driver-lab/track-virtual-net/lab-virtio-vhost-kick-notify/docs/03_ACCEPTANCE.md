# 03_ACCEPTANCE

## 最低通过

- `vhost=off` guest 能 ping host bridge IP
- `vhost=on` guest 能 ping host bridge IP
- records 中有两轮 QEMU 参数记录
- records 中有两轮 host state 记录

## 标准通过

在最低通过基础上，再满足：

- 能确认 `/dev/vhost-net` 存在
- 能确认 `vhost_net` 模块状态
- 有 `vhost=off` 与 `vhost=on` 的对照说明
- 有 kick/notify 路径说明

## 优秀通过

- 能画出 userspace backend vs vhost backend 两张路径图
- 能说明 QEMU 在 `vhost=on` 时仍负责控制面/设备模拟等职责
- 能解释 eventfd/kick/call 与 virtqueue 事件推进的关系

## 推荐记录

每轮都记录：

```bash
ip -br link
ip addr
bridge link
bridge fdb show
lsmod | grep -E 'vhost|tun|bridge'
ls -l /dev/vhost-net /dev/net/tun
ps -ef | grep qemu
```