# USER_GUIDE & ACCEPTANCE

## 快速开始

```bash
cd linux-driver-lab/netdev/stage00_bootstrap
make all
```

## 常用命令

```bash
make discover-paths   # 自动扫描路径
make check-host       # 检查主机工具
make report          # 生成报告
make all             # 执行全部
```

## 怎么判断过了

`output/stage00_report.md` 中 `STAGE00_READY=yes`。

---

## 输出产物

| 产物文件 | 说明 |
|---------|------|
| `output/discovered_paths.env` | 所有路径变量 |
| `output/host_tools.txt` | 工具检查结果（OK/MISSING） |
| `output/stage00_report.md` | 验收报告（PASS/FAIL + 建议） |

---

## 验收标准

### 环境验收

- [ ] `make all` 执行成功，无报错
- [ ] `output/discovered_paths.env` 包含所有路径变量
- [ ] `output/host_tools.txt` 显示工具状态（OK/MISSING）
- [ ] `output/stage00_report.md` 存在且 `STAGE00_READY=yes`

### 路径验收

- [ ] `TARGET_ARCH` 正确（host/x86_64/arm64）
- [ ] `QEMU_X86_64` 或 `QEMU_AARCH64` 存在
- [ ] `CC_HOST` 指向可用 gcc

### 工具链验收

- [ ] gcc / make / ip / ethtool 可用
- [ ] QEMU x86_64 和/或 aarch64 可用
- [ ] 如果 TARGET_ARCH=arm64，cross-compiler 可用

---

## 手动覆盖路径

如果自动发现失败：

```bash
cp env/local.example.env env/local.env
vim env/local.env  # 填入实际路径
```
