# 04_BUILD_AND_RUN_FLOW

## host 路径

适合：
- 先跑脚本
- 先检查 `KDIR`
- 先尝试 stage04 模块构建

主要命令：

```bash
make resolve-host
make build-stage04-host
```

## qemu-x86_64 路径

适合：
- 先把 QEMU run 习惯和 env 参数整理出来
- 不急着上 ARM64 时做过渡验证

主要命令：

```bash
make resolve-x86
make matrix
```

## qemu-arm64 路径

适合：
- 完成正式迁移
- 输出真正的迁移收口报告

主要命令：

```bash
make resolve-arm64
make dryrun-arm64
make build-stage04-arm64
```

## 为什么先 dry-run

因为 QEMU/ARM64 最容易卡在：
- 路径
- 参数
- 镜像
- 工具链

先把命令行和环境解析生成出来，再去真机执行，可以把问题压缩到最小。
