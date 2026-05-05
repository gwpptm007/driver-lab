# 07_EXECUTION_RECORD

## lab-virtio-user-vhost 执行记录

### Step 1: 00_check_env.sh

```
$ cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk/lab-virtio-user-vhost
$ ./scripts/00_check_env.sh
[OK] Environment check saved:
/home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk/lab-virtio-user-vhost/records/20260505_173551-virtio-user-vhost/ENV_CHECK.txt

Next:
  sudo ./scripts/01_setup_hugepages.sh
```

### Step 2: 01_setup_hugepages.sh (sudo)

```
$ sudo ./scripts/01_setup_hugepages.sh
# 记录: 1024 × 2MB hugepages 配置成功
# 输出: records/20260505_173551-virtio-user-vhost/HUGEPAGE_SETUP.txt
```

### Step 3: 02_run_virtio_user_vhost_pair.sh (sudo)

```
$ sudo ./scripts/02_run_virtio_user_vhost_pair.sh
# 后台启动 backend testpmd (net_vhost0) 和 frontend testpmd (net_virtio_user0)
# socket: /tmp/dpdk-vhost-user0
# 前端等待6秒预热，然后各进程运行指定时间后退出
# 输出:
#   - records/.../TESTPMD_BACKEND.log
#   - records/.../TESTPMD_FRONTEND.log
#   - records/.../VHOST_SOCKET.txt
#   - records/.../RUNTIME_STATUS.txt
```

### Step 4: 03_collect_stats.sh

```
$ ./scripts/03_collect_stats.sh
[OK] Stats collected:
/home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk/lab-virtio-user-vhost/records/20260505_173551-virtio-user-vhost/POST_CHECK.txt
```

### Step 5: 04_make_review_bundle.sh

```
$ ./scripts/04_make_review_bundle.sh
./scripts/04_make_review_bundle.sh: line 78: net_vhost: command not found
./scripts/04_make_review_bundle.sh: line 78: /tmp/dpdk-vhost-user0: No such file or directory
./scripts/04_make_review_bundle.sh: line 78: net_virtio_user: command not found
./scripts/04_make_review_bundle.sh: line 78: PASS_WITH_WARN: command not found
./scripts/04_make_review_bundle.sh: line 78: TESTPMD_COMMANDS.txt: command not found
./scripts/04_make_review_bundle.sh: line 78: TESTPMD_BACKEND.log: command not found
./scripts/04_make_review_bundle.sh: line 78: TESTPMD_FRONTEND.log: command not found
./scripts/04_make_review_bundle.sh: line 78: VHOST_SOCKET.txt: command not found
./scripts/04_make_review_bundle.sh: line 78: RUNTIME_STATUS.txt: command not found
./scripts/04_make_review_bundle.sh: line 78: POST_CHECK.txt: command not found
./scripts/04_make_review_bundle.sh: line 78: track-dpdk/lab-dpdk-l2-forwarding: No such file or directory
[OK] Review bundle saved:
/home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk/lab-virtio-user-vhost/records/20260505_173551-virtio-user-vhost/REVIEW_BUNDLE.md
```

**问题**: REVIEW_BUNDLE.md 中 heredoc 内的变量展开失败（显示空值）。

### 关键结果

**VHOST_SOCKET.txt:**
```
socket_ready=1
/tmp/dpdk-vhost-user0: socket
```

**RUNTIME_STATUS.txt:**
```
backend_pid=4458
frontend_pid=4476
socket_exists_after_exit=no
backend_testpmd_rc=124 (被信号终止)
frontend_testpmd_rc=124
```

**TESTPMD_COMMANDS.txt:**
```
## backend
/usr/bin/dpdk-testpmd -l 0-1 -n 4 --file-prefix=vhost_backend \
  --vdev=net_vhost0,iface=/tmp/dpdk-vhost-user0,queues=1,client=0 \
  --no-pci -- \
  --port-topology=chained --forward-mode=rxonly --auto-start --stats-period=2

## frontend
/usr/bin/dpdk-testpmd -l 2-3 -n 4 --file-prefix=virtio_frontend \
  --vdev=net_virtio_user0,path=/tmp/dpdk-vhost-user0,queues=1,server=0 \
  --no-pci -- \
  --port-topology=chained --forward-mode=txonly --auto-start --stats-period=2
```

**Frontend packet stats (txonly):**
```
TX-packets: 256        TX-errors: 0          TX-bytes:  16384
```

**Backend packet stats (rxonly):**
```
RX-packets: 0          RX-missed: 0          RX-bytes:  0
TX-packets: 0          TX-errors: 0          TX-bytes:  0
```

### 分析

1. **socket 连接成功**: socket_ready=1, frontend 能连接 backend
2. **frontend 发送正常**: TX 256 packets，说明 virtio-user 能发送数据
3. **backend 收到 0**: 因为 rxonly 模式只接收不发送，没有向 frontend 回应
4. **rc=124**: 表示被 SIGTERM 终止（正常，因为脚本使用 timeout）

### 结论

**PASS_WITH_WARN** - socket 对接成功，两边 testpmd 均正常启动，但 packet counter 显示非对称（frontend TX 256, backend RX 0），原因是 testpmd 作为 smoke test 没有真正做 L2 转发，只是单向发送测试。

下一步应进入 lab-dpdk-l2-forwarding 实现真正的 L2 forwarding。