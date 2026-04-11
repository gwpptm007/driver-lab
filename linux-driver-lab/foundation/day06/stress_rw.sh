#!/bin/sh
set -u

#
# 脚本做 Day06 的第二项核心验收：并发读写压力测试
#
# 当前驱动的模型不是“无限队列”，而是“单槽 pending work”：
# - 同时只能有一个 work 在处理中
# - 新的 write/ioctl(SET) 若撞上已有 pending work，会返回 -EBUSY
#
# 所以压测的目标不是追求 100% 成功率，而是验证：
# - 并发下没有崩溃
# - 没有 Oops/UAF/leak 痕迹
# - 除了 EBUSY/timeout 这两类预期结果外，没有其他异常错误
#

DURATION=${1:-300}
BASE=/tmp/stress_rw
STOP_FILE="$BASE/stop"

mkdir -p "$BASE"
rm -f "$STOP_FILE" "$BASE"/*.summary "$BASE"/*.log

echo "[day06] concurrent read/write stress start, duration=${DURATION}s"

writer_loop()
{
    ID=$1
    OK=0
    BUSY=0
    ERR=0
    IDX=0

    #
    # writer 做两件事：
    # 1. write 字符串
    # 2. ioctl set 整数
    # 同时覆盖 write 路径和 ioctl(SET) 路径
    #
    while [ ! -f "$STOP_FILE" ]; do
        if [ $((IDX % 2)) -eq 0 ]; then
            /bin/test write "writer${ID}_${IDX}" >>"$BASE/writer${ID}.log" 2>&1
        else
            /bin/test set "$IDX" >>"$BASE/writer${ID}.log" 2>&1
        fi
        RC=$?

        case "$RC" in
            0)
                OK=$((OK + 1))
                ;;
            16)
                # 16 对应 EBUSY：当前 work 还没处理完，属于预期竞争结果
                BUSY=$((BUSY + 1))
                ;;
            *)
                ERR=$((ERR + 1))
                ;;
        esac

        IDX=$((IDX + 1))
    done

    echo "$OK $BUSY $ERR" > "$BASE/writer${ID}.summary"
}

reader_loop()
{
    ID=$1
    OK=0
    TIMEOUT=0
    ERR=0

    #
    # reader 不用普通 read，而是用 read_timeout 2
    #
    # 原因：
    # - 普通 read 在某些时刻可能一直睡眠
    # - 压测脚本需要“最终一定能退出”
    # 允许 reader 周期性超时返回，然后继续下一轮
    #
    while [ ! -f "$STOP_FILE" ]; do
        /bin/test read_timeout 2 >>"$BASE/reader${ID}.log" 2>&1
        RC=$?

        case "$RC" in
            0)
                OK=$((OK + 1))
                ;;
            124)
                # 124 是 test.c 里约定的“用户态读超时”退出码
                TIMEOUT=$((TIMEOUT + 1))
                ;;
            *)
                ERR=$((ERR + 1))
                ;;
        esac
    done

    echo "$OK $TIMEOUT $ERR" > "$BASE/reader${ID}.summary"
}

# 启动 2 个 writer + 2 个 reader
writer_loop 1 &
W1=$!
writer_loop 2 &
W2=$!
reader_loop 1 &
R1=$!
reader_loop 2 &
R2=$!

#
# 主线程只负责“计时”和“发停止信号”
#
START_TS=$(date +%s)
END_TS=$(( START_TS + DURATION ))
NEXT_PROGRESS=$(( START_TS + 5 ))
while [ "$(date +%s)" -lt "$END_TS" ]; do
    NOW=$(date +%s)
    if [ "$NOW" -ge "$NEXT_PROGRESS" ]; then
        ELAPSED=$(( NOW - START_TS ))
        echo "[day06] stress running: ${ELAPSED}s/${DURATION}s"
        NEXT_PROGRESS=$(( NEXT_PROGRESS + 5 ))
    fi
    sleep 1
done

touch "$STOP_FILE"

echo "[day06] stop requested, waiting workers to exit"

# writer 一般会很快退出，先回收 writer
wait "$W1"
wait "$W2"

# reader 可能正阻塞在 read_timeout 里，给它们一点时间自行超时返回
READER_GRACE=5
while [ "$READER_GRACE" -gt 0 ]; do
    R1_ALIVE=0
    R2_ALIVE=0
    kill -0 "$R1" 2>/dev/null && R1_ALIVE=1
    kill -0 "$R2" 2>/dev/null && R2_ALIVE=1

    if [ "$R1_ALIVE" -eq 0 ] && [ "$R2_ALIVE" -eq 0 ]; then
        break
    fi

    sleep 1
    READER_GRACE=$((READER_GRACE - 1))
done

# 如果 reader 还在，就显式发 TERM，避免主脚本永久卡在 wait 上
kill "$R1" "$R2" 2>/dev/null || true

wait "$R1"
wait "$R2"

# 读取各子进程最终统计
[ -f "$BASE/writer1.summary" ] || echo "0 0 1" > "$BASE/writer1.summary"
[ -f "$BASE/writer2.summary" ] || echo "0 0 1" > "$BASE/writer2.summary"
[ -f "$BASE/reader1.summary" ] || echo "0 0 0" > "$BASE/reader1.summary"
[ -f "$BASE/reader2.summary" ] || echo "0 0 0" > "$BASE/reader2.summary"

read W1_OK W1_BUSY W1_ERR < "$BASE/writer1.summary"
read W2_OK W2_BUSY W2_ERR < "$BASE/writer2.summary"
read R1_OK R1_TO R1_ERR < "$BASE/reader1.summary"
read R2_OK R2_TO R2_ERR < "$BASE/reader2.summary"

TOTAL_ERR=$((W1_ERR + W2_ERR + R1_ERR + R2_ERR))

echo "[day06] writer1: ok=${W1_OK} busy=${W1_BUSY} err=${W1_ERR}"
echo "[day06] writer2: ok=${W2_OK} busy=${W2_BUSY} err=${W2_ERR}"
echo "[day06] reader1: ok=${R1_OK} timeout=${R1_TO} err=${R1_ERR}"
echo "[day06] reader2: ok=${R2_OK} timeout=${R2_TO} err=${R2_ERR}"

if [ "$TOTAL_ERR" -ne 0 ]; then
    echo "[FAIL] stress found unexpected errors: total_err=${TOTAL_ERR}"
    exit 1
fi

echo "[PASS] concurrent stress completed: duration=${DURATION}s"
exit 0
