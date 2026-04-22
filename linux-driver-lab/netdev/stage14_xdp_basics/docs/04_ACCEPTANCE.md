# 04_ACCEPTANCE — stage14 通过标准

## 通过标准

| 测试项 | 条件 |
|--------|------|
| smoke test PASS | 收发包正常，无 Bad page state 错误 |
| `ip link set dev nds14s xdp obj xdp_prog.o` | 加载成功，无报错 |
| `ip link show nds14s` | 显示 `xdp` 标志 |
| 发送数据包 | `xdp_pass` 计数增长 |
| `ip link set dev nds14s xdp off` | 卸载 XDP program 成功 |
| `ethtool -S nds14s` | 显示 xdp_pass / xdp_drop / xdp_tx / xdp_redirect 统计 |
| debugfs xdp | `cat /sys/kernel/debug/netdev_stage14_soft/xdp` 显示 XDP 状态 |
| rmmod | 正常卸载，无 hang |

---

## 依赖工具

### 编译驱动

```bash
sudo apt install build-essential linux-headers-$(uname -r)
```

### 编译 BPF 程序

```bash
sudo apt install clang llvm
cd bpf/
./build_xdp.sh
```

---

## XDP 验证命令

### 基础验证

```bash
# 1. 安装 clang（如果需要编译 BPF object）
sudo apt install clang llvm

# 2. 编译 BPF object
cd bpf/ && ./build_xdp.sh && cd ..

# 3. 加载模块
insmod netdev_stage14_soft.ko
ip link show nds14s

# 4. 检查 XDP 功能是否注册
ip link set dev nds14s xdp obj bpf/xdp_pass_kern.o sec xdp_pass
ip link show nds14s  # 应该看到 xdp 标志

# 5. 发送数据包，验证 xdp_pass 增长
./smoke.sh
ethtool -S nds14s | grep xdp_pass

# 6. 卸载 XDP program
ip link set dev nds14s xdp off
ip link show nds14s  # 应该不再显示 xdp 标志
```

---

### XDP DROP 验证（需要 XDP program 支持）

```bash
# 编译 XDP DROP program（返回 XDP_DROP）
clang -target bpf -O2 -c xdp_drop.c
ip link set dev nds14s xdp obj xdp_drop.o sec droptest

# 发送数据包
./smoke.sh

# 检查 xdp_drop 计数
ethtool -S nds14s | grep xdp_drop

# 卸载
ip link set dev nds14s xdp off
```

---

## 调试方法

### dmesg 查看日志

```bash
dmesg | grep -E "stage14|XDP"
```

### debugfs 查看详细状态

```bash
cat /sys/kernel/debug/netdev_stage14_soft/xdp
cat /sys/kernel/debug/netdev_stage14_soft/offload
cat /sys/kernel/debug/netdev_stage14_soft/stats
```

---

## smoke test

```bash
./scripts/smoke.sh
```

期望输出：无 Bad page state warning，smoke test PASS。
