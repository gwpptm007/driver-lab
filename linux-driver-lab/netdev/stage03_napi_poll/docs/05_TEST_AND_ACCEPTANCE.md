# 05_TEST_AND_ACCEPTANCE

## 最小验收目标

### 功能层
- 模块可编译、可加载、可卸载
- `nds3` 能 `ip link set up`
- sender / receiver 能工作
- `rx_mode=direct` 下可以收到环回帧
- `rx_mode=napi` 下可以收到环回帧

### 观测层
- debugfs 统计可读
- `rx_mode=napi` 时：
  - `napi_schedule_count > 0`
  - `napi_poll_count > 0`
  - `napi_complete_count > 0`
  - `pending_enqueued > 0`
  - `pending_drained > 0`
  - `irq_raised > 0`
- 做 burst 测试时，最好还能观察到：
  - `pending_peak > 1`
  - 或 `napi_budget_exhaust_count > 0`

## 推荐测试命令

### 1. direct 模式
```bash
make build-userspace
make build-module
sudo IFNAME=nds3 RX_MODE=direct make load
sudo ip link set nds3 up
sudo ./tools/recv_stage03_frame nds3 0x88B6 8 5
sudo ./tools/send_stage03_frame nds3 hello_direct 0x88B6 8 0
sudo cat /sys/kernel/debug/netdev_stage03/stats
sudo make unload
```

### 2. napi 模式
```bash
sudo IFNAME=nds3 RX_MODE=napi NAPI_WEIGHT=8 make load
sudo ip link set nds3 up
sudo ./tools/recv_stage03_frame nds3 0x88B6 32 5
sudo ./tools/send_stage03_frame nds3 hello_napi 0x88B6 32 0
sudo cat /sys/kernel/debug/netdev_stage03/stats
sudo make unload
```

## smoke 脚本的目标

`make smoke` 不是性能测试，它只做这几件事：
- 接口拉起
- 开 receiver
- burst 发包
- 等 receiver 退出
- 收集 ip link / debugfs 统计

## 不应过度解读的地方

如果 `budget_exhausted == 0`，不代表 NAPI 没工作。

只说明：
- 当前 burst 不够大
- 或 `napi_weight` 设得较大
- 或 poll 很快把 queue 清空了

真正关键的是：
- schedule / poll / complete 是否出现
- pending queue 是否真实参与过数据路径
