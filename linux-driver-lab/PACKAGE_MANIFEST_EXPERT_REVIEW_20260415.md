# PACKAGE_MANIFEST_EXPERT_REVIEW_20260415

本次“专家评审口径”补充内容如下：

## 新增文档

1. `POST_DAY35_MASTER_REVIEW_AND_ROADMAP.md`
2. `docs/EXPERT_REVIEW_CURRENT_BASELINE.md`
3. `docs/ARCHITECTURE_LAYERING_EXPERT.md`
4. `docs/COMPLETION_MATRIX_EXPERT.md`
5. `docs/NEXT_STEP_EXECUTION_PLAN_EXPERT.md`
6. `PACKAGE_MANIFEST_EXPERT_REVIEW_20260415.md`

## 更新文档

1. `README.md`
2. `START_HERE_CURRENT.md`

## 本次包的目标

- 把“最新完整项目”的专家评审结论补成正式文档
- 修复原来顶层引用缺失的总评文件
- 提供架构分层图、完成度矩阵和下一步执行计划
- 保持原工程源码、records、output、脚本结构不变

## 推荐阅读顺序

1. `START_HERE_CURRENT.md`
2. `POST_DAY35_MASTER_REVIEW_AND_ROADMAP.md`
3. `docs/EXPERT_REVIEW_CURRENT_BASELINE.md`
4. `docs/ARCHITECTURE_LAYERING_EXPERT.md`
5. `docs/COMPLETION_MATRIX_EXPERT.md`
6. `docs/NEXT_STEP_EXECUTION_PLAN_EXPERT.md`

## 本轮继续补充（stage06/stage07 规划）

### 新增文档

1. `netdev/stage06_arm64_migration/docs/STAGE06_CLOSEOUT_EXECUTION_CHECKLIST.md`
2. `netdev/stage06_arm64_migration/docs/STAGE06_ACCEPTANCE_CHECKLIST.md`
3. `netdev/stage06_arm64_migration/docs/STAGE06_MIGRATION_MAPPING.md`
4. `netdev/stage06_arm64_migration/docs/STAGE06_KNOWN_ISSUES.md`
5. `netdev/docs/10_STAGE06_CLOSEOUT_AND_STAGE07_DIRECTION.md`
6. `netdev/stage06_arm64_migration/reports/README.md`

### 更新文档

1. `netdev/stage06_arm64_migration/README.md`
2. `netdev/stage06_arm64_migration/START_HERE.md`
3. `netdev/stage06_arm64_migration/TASKS.md`
4. `netdev/README.md`
5. `netdev/docs/00_START_HERE.md`

### 本轮目标

- 把“下一步先做什么”正式沉淀到工程里
- 把 stage06 的收口动作写成可执行清单
- 把 stage06 的验收、迁移映射、已知问题独立成文
- 明确 stage07 的推荐方向，避免后续推进跑偏


## 2026-04-15 追加更新（stage07）

新增：
- `linux-driver-lab/netdev/stage07_real_queue_model/README.md`
- `linux-driver-lab/netdev/stage07_real_queue_model/START_HERE.md`
- `linux-driver-lab/netdev/stage07_real_queue_model/TASKS.md`
- `linux-driver-lab/netdev/stage07_real_queue_model/DIRECTORY_TREE.md`
- `linux-driver-lab/netdev/stage07_real_queue_model/docs/*`
- `linux-driver-lab/netdev/stage07_real_queue_model/driver/*`
- `linux-driver-lab/netdev/stage07_real_queue_model/include/*`
- `linux-driver-lab/netdev/stage07_real_queue_model/scripts/*`
- `linux-driver-lab/netdev/stage07_real_queue_model/tools/README.md`
- `linux-driver-lab/netdev/stage07_real_queue_model/records/README.md`
- `linux-driver-lab/netdev/stage07_real_queue_model/reports/README.md`
- `linux-driver-lab/netdev/stage07_real_queue_model/workdir/README.md`
- `linux-driver-lab/netdev/docs/11_STAGE07_REAL_QUEUE_MODEL_EXECUTION_PLAN.md`

更新：
- `linux-driver-lab/netdev/README.md`
- `linux-driver-lab/netdev/docs/00_START_HERE.md`
- `linux-driver-lab/README.md`

## 2026-04-15 追加更新（stage07 v1 落地）

本轮不是只保留 stage07 目录骨架，而是把第一版真实实现补进工程：

新增/补强：
- `linux-driver-lab/netdev/stage07_real_queue_model/driver/netdev_stage07.c`
- `linux-driver-lab/netdev/stage07_real_queue_model/include/netdev_stage07_compat.h`
- `linux-driver-lab/netdev/stage07_real_queue_model/tools/send_stage07_frame.c`
- `linux-driver-lab/netdev/stage07_real_queue_model/tools/recv_stage07_frame.c`
- `linux-driver-lab/netdev/stage07_real_queue_model/tools/Makefile`
- `linux-driver-lab/netdev/stage07_real_queue_model/scripts/build.sh`
- `linux-driver-lab/netdev/stage07_real_queue_model/scripts/run.sh`
- `linux-driver-lab/netdev/stage07_real_queue_model/scripts/smoke.sh`
- `linux-driver-lab/netdev/stage07_real_queue_model/scripts/stats_check.sh`
- `linux-driver-lab/netdev/stage07_real_queue_model/scripts/trace_smoke.sh`

更新：
- `linux-driver-lab/netdev/stage07_real_queue_model/README.md`
- `linux-driver-lab/netdev/stage07_real_queue_model/START_HERE.md`
- `linux-driver-lab/netdev/stage07_real_queue_model/TASKS.md`
- `linux-driver-lab/netdev/stage07_real_queue_model/driver/README.md`
- `linux-driver-lab/netdev/stage07_real_queue_model/docs/00_USER_GUIDE.md`
- `linux-driver-lab/README.md`

本轮目标：
- 让 stage07 从“规划/骨架”进入“第一版核心代码已落地”状态
- 在代码里明确 submit / notify / complete / post / consume / refill 六段生命周期
- 补齐 debugfs 观测、userspace tools 与 smoke 脚本


## 2026-04-16 追加更新（stage08）

本轮开始 stage08，新增：

- `linux-driver-lab/netdev/stage08_async_backend_transport/README.md`
- `linux-driver-lab/netdev/stage08_async_backend_transport/START_HERE.md`
- `linux-driver-lab/netdev/stage08_async_backend_transport/TASKS.md`
- `linux-driver-lab/netdev/stage08_async_backend_transport/DIRECTORY_TREE.md`
- `linux-driver-lab/netdev/stage08_async_backend_transport/docs/*`
- `linux-driver-lab/netdev/stage08_async_backend_transport/driver/Makefile`
- `linux-driver-lab/netdev/stage08_async_backend_transport/driver/netdev_stage08.c`
- `linux-driver-lab/netdev/stage08_async_backend_transport/include/*`
- `linux-driver-lab/netdev/stage08_async_backend_transport/scripts/*`
- `linux-driver-lab/netdev/stage08_async_backend_transport/tools/*`
- `linux-driver-lab/netdev/docs/12_STAGE08_ASYNC_BACKEND_TRANSPORT_EXECUTION_PLAN.md`

更新：
- `linux-driver-lab/netdev/README.md`
- `linux-driver-lab/netdev/docs/00_START_HERE.md`

本轮目标：
- 以 stage07 收尾为前提，启动 stage08
- 让下一阶段不只是文档，而是进入“已建仓 + 第一版异步 backend 代码已落地”状态
- 明确 stage08 的 front/back 边界、doorbell 语义与 timeline 观测方向
