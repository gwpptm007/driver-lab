#!/bin/sh
set -u

#
# all.sh 是 Day06 的一键验收入口
#
# 完成三件事：
# 1. 反复装卸 500 次
# 2. 加载模块后跑 5 分钟并发压测
# 3. 扫 dmesg 看内核有没有留下异常痕迹
#
# 优先跑这个脚本，
# 再根据失败点去单独跑 insmod_rmmod.sh / stress_rw.sh
#

LOOPS=${1:-500}
DURATION=${2:-300}

echo "[day06] full validation start: loops=${LOOPS}, duration=${DURATION}s"

# init 会先预加载 demo.ko，方便进 guest 后立刻手测
# 回归测试从“干净状态”开始，因此先卸掉一次
if [ -e /sys/module/demo ]; then
    echo "[day06] demo is preloaded by init, reset to clean state first"
    rmmod demo || {
        echo "[FAIL] failed to remove preloaded demo module"
        exit 1
    }
fi

/bin/insmod_rmmod.sh "$LOOPS" || exit 1

# 装卸回归结束后，再单独加载一次，并发压测使用
insmod /demo.ko || {
    echo "[FAIL] insmod before stress failed"
    exit 1
}

/bin/stress_rw.sh "$DURATION"
RC=$?

# 无论压测成败，都先卸载模块
rmmod demo >/dev/null 2>&1

if [ "$RC" -ne 0 ]; then
    echo "[FAIL] stress stage failed"
    exit 1
fi

/bin/check_dmesg.sh || exit 1

echo "[PASS] day06 full validation completed"
exit 0
