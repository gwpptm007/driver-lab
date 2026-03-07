#!/bin/sh
set -u

#
# 核心验收：
# 连续多轮 “装载 -> 基础功能检查 -> 卸载”
#
# 为什么要这样测？
# 因为很多驱动不是第一次 insmod 有问题，而是：
# - 第 50 次以后资源没释放干净
# - workqueue 没同步结束就 rmmod
# - /dev 节点残留
# - init/exit 路径存在偶发 race
#
LOOPS=${1:-500}
I=1

# 临时文件保存失败的用户态输出，方便排错
SET_LOG=/tmp/loop_set.log
GET_LOG=/tmp/loop_get.log
READ_LOG=/tmp/loop_read.log

echo "[day06] insmod/rmmod regression start, loops=${LOOPS}"

#
# guest 的 init 脚本会先自动 insmod 一次 demo.ko，
# 正式跑回归前，先把环境重置成“未加载”状态
#
if [ -e /sys/module/demo ]; then
    echo "[day06] demo already loaded, removing it before regression"
    rmmod demo || {
        echo "[FAIL] failed to remove preloaded demo module"
        exit 1
    }
fi

while [ "$I" -le "$LOOPS" ]; do
    # 第一步：加载模块，验证 init 路径
    insmod /demo.ko || {
        echo "[FAIL] insmod failed at loop $I"
        exit 1
    }

    # 第二步：设备节点是否真的创建出来了
    if [ ! -e /dev/demo ]; then
        echo "[FAIL] /dev/demo missing at loop $I"
        rmmod demo >/dev/null 2>&1
        exit 1
    fi

    # 第三步：做一轮最小功能闭环。
    # set 触发 ioctl + 异步 work；
    # get 验证 value 状态；
    # read_timeout 验证 reader 能被唤醒拿到处理结果
    /bin/test set "$I" >"$SET_LOG" 2>&1
    RC=$?
    if [ "$RC" -ne 0 ]; then
        echo "[FAIL] ioctl set failed at loop $I, rc=$RC"
        cat "$SET_LOG"
        rmmod demo >/dev/null 2>&1
        exit 1
    fi

    /bin/test get >"$GET_LOG" 2>&1
    RC=$?
    if [ "$RC" -ne 0 ]; then
        echo "[FAIL] ioctl get failed at loop $I, rc=$RC"
        cat "$GET_LOG"
        rmmod demo >/dev/null 2>&1
        exit 1
    fi

    /bin/test read_timeout 2 >"$READ_LOG" 2>&1
    RC=$?
    if [ "$RC" -ne 0 ]; then
        echo "[FAIL] read_timeout failed at loop $I, rc=$RC"
        cat "$READ_LOG"
        rmmod demo >/dev/null 2>&1
        exit 1
    fi

    # 第四步：卸载模块，验证 exit 路径以及 cancel_work_sync() 是否靠谱
    rmmod demo || {
        echo "[FAIL] rmmod failed at loop $I"
        exit 1
    }

    #
    # devtmpfs 下节点通常会很快消失
    # 加短暂二次确认，避免误把系统还没来得及更新节点
    # 当成真正的卸载残留问题
    #
    if [ -e /dev/demo ]; then
        sleep 1
        if [ -e /dev/demo ]; then
            echo "[FAIL] /dev/demo still exists after rmmod at loop $I"
            exit 1
        fi
    fi

    if [ $((I % 50)) -eq 0 ]; then
        echo "[day06] loop progress: $I/$LOOPS"
    fi

    I=$((I + 1))
done

echo "[PASS] insmod/rmmod regression completed: loops=${LOOPS}"
exit 0
