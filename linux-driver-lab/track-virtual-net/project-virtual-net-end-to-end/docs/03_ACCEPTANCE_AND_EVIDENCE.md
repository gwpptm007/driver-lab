# 03_ACCEPTANCE_AND_EVIDENCE

## 最低通过

- Lab1 / Lab2 / Lab3 都完成
- 有 ping 记录
- 有 FDB 学习证据
- vhost=off/on 对照

## 标准通过

- Lab1 / Lab2 / Lab3 都完成
- guest ping host 成功
- guest A ping guest B 成功
- bridge FDB 双 MAC 学习
- 有 final report

## 优秀通过

- 可作为作品集中的虚拟化网络系统项目
- 能自然接到 DPDK track

---

## 证据收集

### host 拓扑

```bash
ip -br link
ip addr
bridge link
bridge fdb show
lsmod | grep -E 'vhost|tun|bridge'
```

### QEMU 参数

- guest A/B QEMU args
- vhost=off/on args

### workload

- guest ping host
- guest A ping guest B

### 路径说明

- guest-to-host path
- userspace backend vs vhost backend
- guest-to-guest L2 forwarding