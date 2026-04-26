# START_HERE

## 前置条件

必须先完成或至少具备：

- `lab-virtio-tap-bridge-path` 的基础拓扑
- host 上有 `br-vnet0`
- host 上有 `tap-vnet0`
- QEMU guest 可以通过 virtio-net ping 通 host bridge IP

推荐复用：

```text
host br-vnet0: 192.168.100.1/24
guest eth0:    192.168.100.2/24
tap:           tap-vnet0
```

## 最小开工流程

### 1. 检查环境

```bash
./scripts/check_env.sh
```

### 2. 创建本轮记录目录

```bash
REC=$(./scripts/bootstrap_record_dir.sh)
echo "$REC"
```

### 3. 确认基础 tap/bridge 状态

```bash
./scripts/collect_vhost_state.sh "$REC/before"
```

### 4. 生成 `vhost=off` QEMU 参数

```bash
./scripts/generate_qemu_vhost_args.sh off
```

把输出接入你的 QEMU 启动命令，启动 guest，guest 内配置 IP，然后跑 ping。

### 5. 采集 `vhost=off` 记录

```bash
./scripts/collect_mode_state.sh off "$REC"
```

### 6. 生成 `vhost=on` QEMU 参数

```bash
./scripts/generate_qemu_vhost_args.sh on
```

重启 guest 或重新启动 QEMU，用 `vhost=on` 再跑同样 workload。

### 7. 采集 `vhost=on` 记录

```bash
./scripts/collect_mode_state.sh on "$REC"
```

### 8. 生成对照摘要

```bash
./scripts/diff_vhost_modes.sh "$REC"
```

## 当前最小通过标准

- `vhost=off` 能 ping
- `vhost=on` 能 ping
- `vhost=on` 时 host 能看到 `/dev/vhost-net` 可用或 `vhost_net` 模块状态
- records 下同时有 off/on 两套状态记录
- `SUMMARY.md` 能解释两者差异
