# 03. 测试与验收

## 一、最小验收目标（M1 对应）

必须满足：

- 模块能注册/注销 `net_device`
- `ip link` 能看到接口
- `ndo_open / ndo_stop / ndo_start_xmit` 可以被解释
- 至少有一条可重复的用户态触发路径
- 至少有一份驱动内部统计输出

## 二、建议测试顺序

### 1. 生成环境报告

```bash
make report
```

### 2. 编译用户态小工具

```bash
make build-userspace
```

### 3. 编译模块（需要内核头文件）

```bash
make build-module
```

### 4. 加载模块

```bash
sudo make load
ip link show nds0
```

### 5. 拉起接口并触发一次 xmit

```bash
sudo ip link set nds0 up
sudo ./tools/send_stage01_frame nds0 hello_stage01
```

### 6. 读取统计

```bash
ip -s link show nds0
sudo cat /sys/kernel/debug/netdev_stage01/stats
```

## 三、这一步真正要证明什么

不是证明“网络能通”，而是证明：

- 用户态一帧以太网报文
- 通过该 netdev 进入内核
- 驱动的 `ndo_start_xmit()` 被实际触发
- 统计和观测链路是完整的
