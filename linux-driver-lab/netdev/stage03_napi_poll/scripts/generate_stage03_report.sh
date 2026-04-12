#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
ENV_FILE="$ROOT_DIR/output/host_env_stage03.env"
REPORT_FILE="$ROOT_DIR/output/stage03_report.md"

if [[ ! -f "$ENV_FILE" ]]; then
    echo "missing $ENV_FILE, run scripts/check_host_env.sh first" >&2
    exit 1
fi

# shellcheck disable=SC1090
source "$ENV_FILE"

USERSPACE_READY=no
MODULE_READY=no
SMOKE_READY=no

[[ "$HAS_GCC" == yes && "$HAS_MAKE" == yes ]] && USERSPACE_READY=yes
[[ "$HAS_KERNEL_HEADERS" == yes && "$HAS_MAKE" == yes ]] && MODULE_READY=yes
[[ "$HAS_IP" == yes && "$HAS_TIMEOUT" == yes && "$USERSPACE_READY" == yes ]] && SMOKE_READY=partial
[[ "$MODULE_READY" == yes && "$SMOKE_READY" == partial ]] && SMOKE_READY=yes

cat > "$REPORT_FILE" <<EOF2
# stage03_napi_poll / report

## 1. 阶段目标

围绕 NAPI / poll 建立教学型批处理闭环：

- 可生成环境报告
- 可编译 sender / receiver 用户态工具
- 在具备匹配内核头文件的情况下可编译模块
- 支持 \`rx_mode=direct|napi\` 两模式 smoke 前提

## 2. 当前环境

- uname -r: $UNAME_R
- gcc: $HAS_GCC
- make: $HAS_MAKE
- ip: $HAS_IP
- ethtool: $HAS_ETHTOOL
- sudo: $HAS_SUDO
- timeout: $HAS_TIMEOUT
- debugfs dir: $HAS_DEBUGFS_DIR
- kernel headers: $HAS_KERNEL_HEADERS
- kdir: $KDIR

## 3. 当前判断

- USERSPACE_READY=$USERSPACE_READY
- MODULE_READY=$MODULE_READY
- SMOKE_READY=$SMOKE_READY

## 4. 结论

EOF2

if [[ "$MODULE_READY" == yes ]]; then
    cat >> "$REPORT_FILE" <<'EOF2'
当前环境具备 stage03 最小落地前提，可以继续执行：

```bash
make build-userspace
make build-module
sudo make load
sudo make smoke
sudo make unload
```
EOF2
else
    cat >> "$REPORT_FILE" <<'EOF2'
当前环境缺少匹配内核头文件，因此无法在本机直接验证模块构建。
这不影响先评审 stage03 的：

- pending queue + NAPI 设计
- direct / napi 两模式对照
- sender / receiver 工具
- 构建 / 加载 / 验收脚本

后续在具备 `/lib/modules/$(uname -r)/build` 的真实开发机上再执行模块构建即可。
EOF2
fi

echo "[stage03] report generated: $REPORT_FILE"
