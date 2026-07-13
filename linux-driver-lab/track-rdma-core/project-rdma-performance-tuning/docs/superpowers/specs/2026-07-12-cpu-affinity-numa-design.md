# CPU Affinity / NUMA Design

## 1. 目标

在 `project-rdma-performance-tuning` 现有 batch / inline / selective / poll / RTT 框架上，补一个保守的“CPU affinity / NUMA 记录阶段”：

- 不改 RDMA 协议和测量口径
- 允许 server/client 通过环境变量选择 CPU 绑定与 NUMA 节点绑定
- 让脚本和进程日志里都留下足够证据，证明这次实验到底是怎么绑的

## 2. 边界

本阶段只做：

- `taskset -c` 级别的 CPU 绑定
- `numactl --cpunodebind= --membind=` 级别的同节点 NUMA 绑定
- 本地 / 双机脚本对这些参数的透传
- 进程运行时把 `current_cpu`、`Cpus_allowed_list`、`Mems_allowed_list` 打出来

本阶段不做：

- 自动寻找“最佳 CPU”
- 自动寻找“最佳 NUMA 组合”
- 把 server/client 的多线程模型改成核间分工
- 把 affinity/NUMA 结果直接写成性能结论

## 3. 方案比较

### 方案 A：脚本层绑定 + 进程层记录（推荐）

- 脚本层用 `taskset` / `numactl` 起进程
- 进程启动时自己把当前 CPU、allowed cpu list、allowed mem list 打日志

优点：

- 改动小，不碰 RDMA 主路径
- 双机脚本、单机 smoke、手工命令都能复用
- 有“启动命令证据”和“进程自报证据”两层记录

缺点：

- 不会自动分析哪种绑定最优

### 方案 B：只做脚本层绑定

优点：

- 最省改动

缺点：

- 日志里看不到进程实际看到的 affinity / mem policy
- 复盘时容易只剩命令，没有运行时证据

### 方案 C：把 affinity 直接做成 sweep 矩阵

优点：

- 更像完整调优实验

缺点：

- 太快进入组合爆炸
- 依赖机器拓扑，当前 135/134 环境不一定有多 NUMA 节点

## 4. 选型

采用方案 A。

原因：

- 这是一个“记录阶段”，不是“自动调优阶段”
- 当前仓库最需要的是把实验变量显式化、可复现化
- 后续若真要扫矩阵，可以在现有入口上叠加，不用返工

## 5. 接口设计

新增环境变量：

- `PERF_SERVER_CPUSET`
- `PERF_CLIENT_CPUSET`
- `PERF_SERVER_NUMA_NODE`
- `PERF_CLIENT_NUMA_NODE`

语义：

- `CPUSET` 传给 `taskset -c`
- `NUMA_NODE` 传给 `numactl --cpunodebind=<node> --membind=<node>`
- 两者都为空时，保持当前行为不变

## 6. 文件落点

- `src/perf_common.h`
  - 新增 affinity / NUMA 环境获取与运行时记录 helper
- `tests/perf_launch_helpers.sh`
  - 新增安全的 launcher 组装函数
- `tests/perf_smoke_test.sh`
  - 本地 smoke 支持 affinity / NUMA
- `tests/dual_perf_server.sh`
  - 双机 server 脚本支持 affinity / NUMA
- `tests/dual_perf_client.sh`
  - 双机 client 脚本支持 affinity / NUMA
- `tests/check_env.sh`
  - 输出 `lscpu` / `numactl --hardware` / 相关 env
- `README.md` / `docs/*.md` / `tests/TEST_RECORD_*.md`
  - 补原理、命令、记录

## 7. 验证策略

最小 fresh 验证：

1. 135 上重新 `make`
2. 135 上跑 `envcheck`
3. 135 上跑一组绑核 sample，例如：
   - `PERF_SERVER_CPUSET=0`
   - `PERF_CLIENT_CPUSET=1`
4. grep：
   - `perf_binding role=server`
   - `perf_binding role=client`
   - `cpus_allowed=`
   - `mems_allowed=`
   - 原有 PASS marker

如果 135 只有单 NUMA 节点，也照实记录为“只有 node0，可做记录，不足以形成 NUMA 对比结论”。
