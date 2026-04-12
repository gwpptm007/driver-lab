# tools

## send_stage01_frame

一个最小的 AF_PACKET 原始套接字工具，用来给 `stage01` 的教学型 netdev 发送一帧以太网报文，
从而触发 `ndo_start_xmit()`。

### 构建

```bash
make build-userspace
```

### 使用

```bash
sudo ./tools/send_stage01_frame nds0 hello_stage01
```

### 说明

- 需要 root 权限（原始套接字）
- 本工具不是性能压测工具，只用于最小 lifecycle / xmit 验证
