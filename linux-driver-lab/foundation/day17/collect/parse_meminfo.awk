# parse_meminfo.awk
# ----------------
# 这个 awk 脚本只做一件很小但很稳定的事：
# 从 Linux 的 /proc/meminfo 中，抽出 Day15 约定好的几个字段，
# 并以 key=value 的形式输出，方便宿主机脚本直接 source/读取。
#
# 为什么单独写一个 awk？
# 1. /proc/meminfo 的格式相对固定，适合让 awk 来做文本提取；
# 2. host_collect.sh 就不需要堆太多字符串处理逻辑；
# 3. 后面 D19 比较 baseline / trim1 / trim2 时，也可以重复复用。
#
# 用法：
#   awk -f parse_meminfo.awk meminfo.txt
#
# 输出示例：
#   memtotal_kib=1048576
#   memfree_kib=980000
#   ...

BEGIN {
    wanted["MemTotal"]      = "memtotal_kib"
    wanted["MemFree"]       = "memfree_kib"
    wanted["MemAvailable"]  = "memavailable_kib"
    wanted["Slab"]          = "slab_kib"
    wanted["SReclaimable"]  = "sreclaimable_kib"
    wanted["SUnreclaim"]    = "sunreclaim_kib"
    wanted["KernelStack"]   = "kernelstack_kib"
    wanted["PageTables"]    = "pagetables_kib"

    # 先给默认值。这样即便某个字段在某个内核版本里不存在，
    # 宿主机脚本也不会因为变量未定义而报错。
    values["memtotal_kib"] = 0
    values["memfree_kib"] = 0
    values["memavailable_kib"] = 0
    values["slab_kib"] = 0
    values["sreclaimable_kib"] = 0
    values["sunreclaim_kib"] = 0
    values["kernelstack_kib"] = 0
    values["pagetables_kib"] = 0
}

{
    # /proc/meminfo 行格式一般类似：
    #   MemTotal:       1048576 kB
    # 所以：
    #   $1 = MemTotal:
    #   $2 = 1048576
    key = $1
    sub(/:$/, "", key)

    if (key in wanted) {
        values[wanted[key]] = $2
    }
}

END {
    print "memtotal_kib=" values["memtotal_kib"]
    print "memfree_kib=" values["memfree_kib"]
    print "memavailable_kib=" values["memavailable_kib"]
    print "slab_kib=" values["slab_kib"]
    print "sreclaimable_kib=" values["sreclaimable_kib"]
    print "sunreclaim_kib=" values["sunreclaim_kib"]
    print "kernelstack_kib=" values["kernelstack_kib"]
    print "pagetables_kib=" values["pagetables_kib"]
}
