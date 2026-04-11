# Day17 代码与脚本布局说明

## 1. 为什么要把说明文档和执行脚本分层

你这次的要求是：

- 详细步骤说明写到文档里
- 代码说明放在 day17 目录里
- 测试全流程都在 day17 里闭环

所以 Day17 采用了这样的分层：

### 根目录
放“入口型文档”和“可执行脚本”——你第一次上手时最常用。

### docs/
放“解释型文档”和“步骤型文档”——适合系统学习和回看。

### collect/
放“结果采样脚本”——保持 guest 与 host 的职责清晰。

### config/
放“profile 片段”—— baseline / round1 / round2b 都集中到这里。

---

## 2. 脚本之间的关系

- `apply_config.sh`：只管 profile 和内核配置
- `build.sh`：只管 rootfs / 模块 / DTB / 启动前组装
- `run_qemu.sh`：只管手工启动 guest
- `collect/guest_collect.sh`：只管 guest 内采样
- `collect/host_collect.sh`：只管宿主机自动采样与结果归档

这能保证你排错时更容易判断问题落在哪一层。
