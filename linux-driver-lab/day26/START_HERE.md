# Day26 - START HERE

> 说明：day26 的模块必须通过顶层 `make module` 构建，不要进入 `day26/driver/` 后直接执行 `make`。

## 一句话目标

在 Day25 的 EDU + MSI 成功基础上，把驱动继续做成一个“用户态工具友好”的闭环：

- `info / read-state / count / status` 用于读状态；
- `trigger <value>` 用于触发中断；
- `reset-stats` 用于清零统计；
- `trigger 0` 用于验证清晰错误码。

## 起步

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day26
source env/local.wq7.env
```

## 第一次在本地机器上跑，建议照这个顺序

```bash
# 1) 准备第三方 lspci 源码（只在当前 day26 目录内使用）
mkdir -p third_party
# git clone https://github.com/pciutils/pciutils.git third_party/pciutils

# 2) zip 解压后经常会丢执行位，这两步建议每次都做
chmod +x scripts/*.sh
chmod +x guest/init.day26
chmod +x third_party/pciutils/lib/configure
chmod +x third_party/pciutils/configure 2>/dev/null || true

# 3) 主流程
make build-lspci
make check
make kernel-module-tree
make build-tools
make module
sudo -E make rootfs
sudo chown -R "$USER:$USER" workdir
make backend
sudo -E make run
```

接着按 `docs/01_LOCAL_RUNBOOK.md` 查看结果，按 `docs/02_ACCEPTANCE.md` 判定是否通过。
