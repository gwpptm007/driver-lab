# STAGE06_CLOSEOUT_EXECUTION_CHECKLIST

## 文档定位

这份文档不是阶段背景说明，而是 **stage06 收口执行清单**。  
目标是把当前已经能跑、已经有 ARM64 smoke 结果的 `stage06_arm64_migration`，进一步收成一个真正可交付、可复现、可作为后续阶段基座的阶段节点。

---

## 收口完成后的标准

达到下面这句话，就算 stage06 真正收口：

> 在一台新机器上，只要准备好 kernel-src、busybox、QEMU、交叉工具链，并修改一份 env，就能复现 stage04 在 host / qemu-x86_64 / qemu-arm64 下的环境解析、构建、dry-run、smoke 和差异报告。

换句话说，stage06 完成后必须同时满足：

1. 不依赖个人路径  
2. 不依赖人工记忆步骤  
3. 不依赖作者口头补充  
4. 不把主驱动逻辑和平台脚手架混在一起

---

## 第一优先级：先收环境解析层

### 必改脚本

- `scripts/check_platform_env.sh`
- `scripts/resolve_platform_env.sh`

### 收口目标

把 stage06 从“在作者机器上能跑”改成“在任何准备好的机器上都能跑”。

### 具体动作

#### A. 清掉个人路径 fallback

必须去掉所有类似：

- `/home/wq7/...`
- 固定用户名目录
- 固定本机 kernel build 输出路径
- 固定本机 rootfs 位置

#### B. 统一解析顺序

建议顺序固定为：

1. 显式环境变量  
2. `env/stage06_arm64_migration.env`  
3. 仓库相对路径推导  
4. 系统默认命令发现（如 `command -v`）

#### C. 引入 fail-fast 检查

建议补两个基础函数：

- `require_file`
- `require_dir`

用于在环境解析阶段直接指出：
- 缺哪个文件
- 缺哪个目录
- 当前 profile 为什么不能继续

#### D. 统一 resolved env 契约

`output/resolved_*.env` 至少应稳定输出：

- `TARGET_PROFILE`
- `TARGET_ARCH`
- `RUN_MODE`
- `QEMU_BIN`
- `CROSS_COMPILE`
- `KDIR`
- `KERNEL_BUILD_DIR`
- `KERNEL_IMAGE`
- `ROOTFS_IMAGE`
- `QEMU_MACHINE`
- `QEMU_CPU`
- `QEMU_MEMORY`
- `STAGE04_DIR`
- `IFNAME`
- `RING_SIZE`
- `NAPI_WEIGHT`
- `RX_BUF_SIZE`
- `RESOLVE_OK`
- `RESOLVE_ERROR`

---

## 第二优先级：统一 build / dry-run / smoke 入口

### 当前已有入口

- `scripts/build_stage04_for_target.sh`
- `scripts/dryrun_arm64_qemu.sh`
- `scripts/smoke.sh`

### 收口方向

#### A. build 入口抽象化

建议把：

- `build_stage04_for_target.sh`

抽象成：

- `build_for_target.sh`

并通过变量传入：

- `SOURCE_STAGE=stage04_ring_dma`

这样 stage06 以后不仅能迁 stage04，还能迁 stage07、stage08。

#### B. dry-run 入口通用化

建议把：

- `dryrun_arm64_qemu.sh`

抽象成：

- `dryrun_qemu.sh`

然后按 profile 决定：
- `qemu-x86_64`
- `qemu-arm64`

#### C. smoke 拆分为两层

建议拆成：

- `smoke_static.sh`
- `smoke_runtime.sh`

再由 `smoke.sh` 统一调度。

##### smoke_static 的职责
- 检查 env 是否完整
- 检查 build 产物是否存在
- 检查 dry-run 命令是否可生成
- 检查 report 是否能生成

##### smoke_runtime 的职责
- 起 QEMU / 或执行可运行路径
- 加载模块
- 验证 netdev 注册
- 验证 TX→RX 闭环
- 验证 NAPI poll 触发
- 检查 dmesg 中关键错误

---

## 第三优先级：补齐 3 份关键文档

### 1. `STAGE06_ACCEPTANCE_CHECKLIST.md`
职责：把“阶段完成”写成正式验收表。

### 2. `STAGE06_MIGRATION_MAPPING.md`
职责：讲清：
- 什么保持不变
- 什么是平台差异
- 什么由兼容层承担
- 什么能被 future stage 复用

### 3. `STAGE06_KNOWN_ISSUES.md`
职责：把当前实际踩过的坑沉淀成问题清单，而不是散落在日志和任务文档里。

---

## 第四优先级：Makefile 目标统一

### 建议保留
- `report`
- `matrix`
- `diff`
- `resolve-host`
- `resolve-x86`
- `resolve-arm64`
- `smoke`
- `clean`

### 建议新增
- `validate-env`
- `build-host`
- `build-arm64`
- `dryrun-x86`
- `dryrun-arm64`
- `smoke-static`
- `smoke-runtime`

### 建议弱化
- `build-stage04-host`
- `build-stage04-arm64`

理由：这些目标名把 stage06 永久绑死在 stage04 上，不利于后续复用。

---

## 第五优先级：理清 output / records / reports 边界

### `output/`
只放当前生成物，例如：
- resolved env
- log
- ko
- img
- dry-run shell
- 临时报告草稿

### `records/`
只放阶段证据归档，例如：
- 某次 smoke 结果
- 某次测试的 dmesg
- 当时的固定快照

### `reports/`
建议新增，用于放：
- 阶段报告
- 平台差异报告
- compare 结果
- acceptance 结论

---

## 推荐开工顺序

### Step 1
先改：
- `scripts/check_platform_env.sh`
- `scripts/resolve_platform_env.sh`

### Step 2
再改：
- build 入口
- dry-run 入口
- smoke 入口
- Makefile 目标名

### Step 3
补文档：
- `STAGE06_ACCEPTANCE_CHECKLIST.md`
- `STAGE06_MIGRATION_MAPPING.md`
- `STAGE06_KNOWN_ISSUES.md`

### Step 4
整理目录边界：
- output
- records
- reports

### Step 5
做一轮完整回归：

1. `make validate-env`
2. `make resolve-host`
3. `make resolve-x86`
4. `make resolve-arm64`
5. `make build-host`
6. `make build-arm64`
7. `make dryrun-arm64`
8. `make smoke-static`
9. `make smoke-runtime`
10. `make matrix`
11. `make diff`
12. `make report`

---

## 收口完成判据

只要下面 8 条全部满足，就可以把 stage06 判定为“已收口”：

1. 脚本中不再出现个人路径  
2. host / x86 / arm64 三个平台都能 resolve  
3. build 入口统一  
4. dry-run 入口统一  
5. smoke 分静态和运行两层  
6. 验收 checklist 单独成文  
7. migration mapping 单独成文  
8. output / records / reports 边界清楚
