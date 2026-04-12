# stage01_netdev_skeleton

## 阶段定位

`stage01` 的目标不是一上来做一个“像真实商用网卡一样复杂”的驱动，而是先把
`net_device` 的**注册、打开、关闭、发包入口、最小统计、最小可观测性**做成一个
可执行、可解释、可演示的闭环。

这一阶段故意保持 **架构中立**：

- 不强绑 ARM64
- 不强绑 QEMU
- 不强绑 tap / virtio-net
- 先把 `net_device` 生命周期吃透

## 当前已落地内容

- 一个教学型最小 `net_device` 内核模块：`driver/netdev_stage01.c`
- 最小 `ndo_open / ndo_stop / ndo_start_xmit / ndo_get_stats64` 骨架
- `debugfs` 统计导出：`/sys/kernel/debug/netdev_stage01/stats`
- 一个用户态原始套接字小工具：`tools/send_stage01_frame.c`
- build / load / unload / smoke / report 脚本
- 最小阶段文档、目录说明与验收口径

## 本阶段学什么

### 1. `net_device` 生命周期

重点理解：

- `alloc_etherdev_mqs`
- `register_netdev`
- `unregister_netdev`
- `ndo_open`
- `ndo_stop`
- `ndo_start_xmit`

### 2. “能看到接口”和“能触发驱动入口”是两回事

本阶段最重要的不是复杂数据面，而是把这两件事拆清楚：

- 接口是否成功注册到内核网络栈
- 用户态是否能真正走到 `ndo_start_xmit`

### 3. 可观测性要从第一阶段就建立

这一阶段已经导出最小统计，后面 `stage02~stage04` 会在此基础上逐步扩展。

## 目录

- `driver/`：内核模块源码
- `tools/`：用户态小工具
- `scripts/`：构建、加载、测试、报告脚本
- `env/`：变量化配置
- `docs/`：阶段设计与验收说明
- `output/`：构建产物与报告
- `records/`：运行记录占位
- `workdir/`：临时工作目录

## 建议阅读顺序

1. `docs/01_STAGE_GOAL_AND_BOUNDARY.md`
2. `docs/02_DRIVER_DESIGN.md`
3. `docs/03_TEST_AND_ACCEPTANCE.md`
4. `driver/netdev_stage01.c`
5. `scripts/smoke.sh`

## 最常用命令

```bash
cd linux-driver-lab/netdev/stage01_netdev_skeleton
make report
make build-userspace
make build-module
sudo make load
sudo make smoke
sudo make unload
```

## 说明

在没有当前运行内核头文件的环境里，`build-module` 会失败，这属于预期行为。
这并不影响你先看代码、看脚本、看阶段设计。

用户态工具 `tools/send_stage01_frame` 可以独立编译验证：

```bash
make build-userspace
```
