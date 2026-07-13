# TEST_RECORD_20260713_PHASE5_DYNAMIC_DIRECTORY

## 1. 目标

验证 FNV-1a 动态 key directory 的 PUT/GET，以及不同完整 key 落入同 bucket 时拒绝覆盖原 value 的边界。

## 2. 环境

- 主机：`192.168.65.135`
- RDMA device：`rxe0`
- netdev：`ens34`
- GID index：`1`
- 目录：8 个单槽 bucket，entry 保存 hash、slot、version、state 和完整 key

## 3. 完整命令

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-one-sided-kv
make clean
make
RDMA_NETDEV=ens34 RDMA_GID_ADDR=fe80::34 \
SUDO_PASSWORD='<sudo-password>' make test

grep -E 'KV_DYNAMIC_|KV_DIRECTORY_|kv_directory_' \
  tests/kv-client.log tests/kv-server.log
```

## 4. 关键证据

```text
kv_directory_put key=dynamic-alpha hash=0xc1e0c4f bucket=7 slot=7
KV_DYNAMIC_KEY_PUT_PASS
kv_directory_get key=dynamic-alpha bucket=7 value=dynamic-value-for-dynamic-alpha
KV_DYNAMIC_KEY_GET_PASS
kv_directory_collision existing=dynamic-alpha rejected=dynamic-collision-7 bucket=7
KV_DIRECTORY_COLLISION_PASS
kv_directory_server key=dynamic-alpha hash=0xc1e0c4f bucket=7 slot=7
KV_DYNAMIC_DIRECTORY_PASS
```

PUT 先完成 record WRITE，再发布 directory entry；GET 先读目录确认完整 key，再读 record 并比较完整结构。碰撞路径只发目录 READ，不发覆盖 record 的 WRITE，server 复查原 key 和 checksum 未变化。

## 5. 结论与边界

动态目录和碰撞拒绝 PASS。当前是单槽 bucket，不包含开放寻址、删除、并发原子发布或 crash consistency；这些属于后续分布式 KV 协议扩展。
