# Day07 README 整理与一页复盘

## 今日任务

- 整理仓库根 README
- 补齐 `kernel-src/` 环境说明
- 明确 `build.sh` 的路径规则
- 输出 W1 一页复盘

这一天不再新增驱动能力，重点是把项目整理成“别人拿到后能照着搭环境并复制跑通”的状态。

---

## 这一天补了什么

### 1. README 完整化

根目录 `README.md` 补齐了：

- 仓库总览
- `kernel-src/` 目录骨架的意义
- Windows 宿主机 + VMware + Ubuntu + QEMU 的关系
- Ubuntu 依赖安装
- Linux 5.15.10 / BusyBox 1.36.1 的准备入口
- `build.sh` 的相对路径与旧路径兼容规则
- Day06 的验收入口
- waitqueue / workqueue 的简洁解释
- GitHub 提交建议

### 2. 环境准备文档化

新增并整理：

- `../kernel-src/README.md`
- `../kernel-src/linux-5.15.10/README.md`
- `../kernel-src/busybox-1.36.1/README.md`

重点回答两个问题：

- 为什么仓库里保留 `kernel-src/` 骨架但不提交完整源码
- 别人 clone 仓库后应该如何把环境补齐

### 3. build.sh 可迁移化

`day01 ~ day06/build.sh` 统一改为：

- 优先使用仓库相对路径
- 兼容历史上的 `/home/wq7/workspace/kernel-src/...` 旧路径
- 支持环境变量 `KDIR`、`BUSYBOX_DIR`

这样仓库放到别的目录后也更容易跑。

### 4. W1 一页复盘

输出 `../docs/W1_REVIEW.md`，从四个维度回顾：

- 接口
- 设计
- 风险
- 回归

---

## 推荐阅读顺序

1. `README.md`
2. `../kernel-src/README.md`
3. `../docs/W1_REVIEW.md`
4. `../docs/PROGRESS.md`
5. `../docs/ROADMAP.md`

---

## Day07 验收标准

### 目标

- README 完整
- 环境说明完整
- 路径规则清晰
- 别人可复制跑通

### 验收方式

先准备：

- `../kernel-src/linux-5.15.10`
- `../kernel-src/busybox-1.36.1`

然后进入任意一个 day 目录，例如：

```bash
cd ../day06
chmod +x build.sh
./build.sh
```

进入 guest 后执行：

```sh
/bin/all.sh
```

如果能够按 README 的说明完成环境准备、启动 QEMU、进入 guest 并跑通 Day06 验收脚本，就说明 Day07 的“文档收口”是有效的。

---

## 这一页的作用

这一页不是教新接口，而是提醒自己：

**能跑起来的代码只是第一步。能把目录、依赖、路径、脚本、复盘讲清楚，项目才更像一个完整作品。**
