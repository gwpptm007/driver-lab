# RDMA Memory Region Deep Dive

本项目专门研究 PD、用户缓冲区、MR、access flags、lkey/rkey 和地址对齐，不创建 CQ/QP。

## 阅读顺序

1. `include/mr_lab.h`：环境资源与接口。
2. `src/mr_experiments.c`：五个表驱动 MR 实验。
3. `docs/MEMORY_REGION_MODEL.md`：MR 深度原理与 Mermaid/UML。
4. `docs/ARCHITECTURE.md`：工程和调用路径。
5. `tests/TEST_RECORD_20260701.md`：完整测试命令与真实输出。

## 构建运行

```bash
make clean && make
build/rdma-mr-lab --list
build/rdma-mr-lab --device rxe0 --port 1
make test
```

实验矩阵：

| case | 目的 |
| --- | --- |
| `local_write` | 验证本地写权限 |
| `remote_read_only` | 验证远端读权限可独立注册 |
| `remote_write_without_local_write` | 验证远端写依赖本地写 |
| `all_permissions` | 验证常用完整权限组合 |
| `unaligned_all_permissions` | 观察非页对齐地址的 provider 行为 |

本项目只验证内存注册，不执行远端访问。权限能注册不代表已经建立通信或发生 DMA。
