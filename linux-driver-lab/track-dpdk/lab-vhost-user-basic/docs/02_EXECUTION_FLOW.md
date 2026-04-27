# 02_EXECUTION_FLOW

## 执行流程

```text
00_check_env
  ↓
01_setup_hugepages
  ↓
02_run_vhost_testpmd
  ↓
03_collect_stats
  ↓
04_make_review_bundle
```

## 核心命令模型

脚本会生成类似命令：

```bash
dpdk-testpmd   -l 0-1   -n 4   --file-prefix=vhost_basic   --vdev=net_vhost0,iface=/tmp/dpdk-vhost-user0,queues=1,client=0   --no-pci   --   --port-topology=chained   --forward-mode=io   --auto-start   --stats-period=2
```

## 为什么需要 FIFO

`02_run_vhost_testpmd.sh` 使用 FIFO 给 testpmd 输入命令：

```text
show port info all
show port stats all
stop
quit
```

这样可以在 testpmd 运行期间检查 socket 是否创建，然后再优雅退出。

## 记录目录

每次执行会生成：

```text
records/YYYYMMDD_HHMMSS-vhost-user-basic/
```

重点文件：

- `ENV_CHECK.txt`
- `HUGEPAGE_SETUP.txt`
- `TESTPMD_COMMAND.txt`
- `TESTPMD_VHOST.log`
- `VHOST_SOCKET.txt`
- `RUNTIME_STATUS.txt`
- `POST_CHECK.txt`
- `REVIEW_BUNDLE.md`
