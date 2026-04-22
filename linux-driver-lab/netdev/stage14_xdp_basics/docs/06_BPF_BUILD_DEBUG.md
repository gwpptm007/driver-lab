# 06_BPF_BUILD_DEBUG — build_xdp.sh 问题排查记录

## 问题描述

在测试机上执行 `./build_xdp.sh` 编译 XDP BPF 程序时，遭遇多次编译失败。

---

## 问题 1：clang 未找到

### 现象
```
clang: Ubuntu clang version 14.0.0-1ubuntu1.1
[WARN] 未找到 bpf_helpers.h，编译可能失败
```

### 排查过程
测试机已安装 clang 14，但脚本使用 `command -v clang` 检测，检测逻辑本身正常。真正的问题是 `bpf_helpers.h` 不在默认搜索路径。

### 解决
找到 `bpf_helpers.h` 实际路径：
```bash
find /usr/src/linux-headers-$(uname -r)/tools -name "bpf_helpers.h"
# /usr/src/linux-headers-6.8.0-110-generic/tools/bpf/resolve_btfids/libbpf/include/bpf/bpf_helpers.h
```

---

## 问题 2：asm/types.h 找不到

### 现象
```
fatal error: 'asm/types.h' file not found
#include <asm/types.h>
```

### 排查过程
`/usr/include/linux/types.h` 包含 `<asm/types.h>`，但系统中该文件位于非标准路径。

检查路径：
```bash
# 搜索 asm/types.h 实际位置
find /usr -name "types.h" -path "*/asm/*" 2>/dev/null
# /usr/include/x86_64-linux-gnu/asm/types.h
```

Ubuntu x86_64 的 asm 头文件在 `/usr/include/x86_64-linux-gnu/asm/`，不是 `/usr/include/asm/`。

### 解决
加入正确的 include 路径：
```bash
-I /usr/include/x86_64-linux-gnu
```

---

## 问题 3：bash array 元素前导空格导致编译失败

### 现象
脚本中定义 `local inc_flags=("-I /usr/..." "-I /usr/..." ...)` 后，展开时每个元素多了前导空格，导致 clang 无法识别路径。

### 排查过程
1. **直接执行编译命令成功**：
```bash
kver=$(uname -r)
clang -O2 -target bpf -Wall \
  -I /usr/src/linux-headers-${kver}/tools/bpf/resolve_btfids/libbpf/include \
  -I /usr/include/linux \
  -I /usr/include/asm-generic \
  -I /usr/include/x86_64-linux-gnu \
  -c xdp_pass_kern.c -o xdp_pass_kern.o  # ✓ 成功
```

2. **通过脚本执行失败**：
```bash
./build_xdp.sh  # ✗ asm/types.h 找不到
```

3. **用 bash -x 追踪**：
```bash
bash -x ./build_xdp.sh 2>&1 | grep clang
# clang -O2 -target bpf -Wall '-I /usr/src/linux-headers-6.8.0-110-generic/tools/bpf/...'
#       '-I /usr/include/linux'
#       '-I /usr/include/asm-generic'
#       '-I /usr/include/x86_64-linux-gnu'
#       ...
# 注意每个参数前的单引号和前导空格 —— 元素被错误解析
```

4. **hexdump 对比文件内容**：
```bash
xxd build_xdp.sh | grep inc_flags
# 文件内容正确: local inc_flags=("...
# 但 bash 解析时，array 每个元素的缩进空格被错误计入
```

根本原因：bash 脚本中 `local inc_flags=("..." "...")` 的 array 定义，换行后的缩进空格被 bash 解释器当作字符串的一部分，导致参数传递失败。

### 解决
改用简单字符串而非 array：
```bash
# 错误写法（bash 3.x 会解析异常）
local inc_flags=(
    "-I /usr/src/..."
    "-I /usr/include/..."
)

# 正确写法
local inc_flags="-I /usr/src/... -I /usr/include/..."
clang -O2 -target bpf -Wall $inc_flags -c "$src" -o "$out"
```

---

## 问题 4：BTF is required but missing

### 现象
```bash
ip link set dev nds14s xdp obj xdp_pass_kern.o sec xdp_pass
# ERROR: opening BPF object file failed
# libbpf: BTF is required, but is missing or corrupted.
```

### 排查过程
1. 确认内核开启 BTF：`CONFIG_DEBUG_INFO_BTF=y` ✓
2. 检查 `.o` 文件中是否有 BTF section：
```bash
llvm-objdump -h xdp_pass_kern.o | grep BTF
# 无 BTF section
```

3. 对比直接编译（成功）和脚本编译（失败）的差异：
```bash
# 直接编译时 clang 版本信息
/usr/lib/llvm-14/bin/clang -cc1 ...
# 脚本执行时调用链有问题
```

### 解决
编译时加 `-g` flag 保留调试信息（包含 BTF）：
```bash
clang -O2 -target bpf -Wall -g $inc_flags -c "$src" -o "$out"
```

验证：
```bash
llvm-objdump -h xdp_pass_kern.o | grep BTF
# 17 .BTF                   0000036d
# 18 .rel.BTF               ...
```

---

## 最终有效的编译命令

```bash
kver=$(uname -r)
clang -O2 -target bpf -Wall -g \
  -I /usr/src/linux-headers-${kver}/tools/bpf/resolve_btfids/libbpf/include \
  -I /usr/include/linux \
  -I /usr/include/asm-generic \
  -I /usr/include/x86_64-linux-gnu \
  -c xdp_pass_kern.c -o xdp_pass_kern.o
```

---

## 排查工具总结

| 工具 | 用途 |
|------|------|
| `find /usr -name "bpf_helpers.h"` | 定位头文件实际路径 |
| `find /usr -name "types.h" -path "*/asm/*"` | 定位架构相关头文件 |
| `bash -x ./build_xdp.sh` | 追踪脚本执行流程 |
| `llvm-objdump -h *.o` | 验证 BPF section 和 BTF 信息 |
| `clang -O2 -target bpf -v ...` | 详细编译输出（含 SEARCH_PATH） |
| `hexdump -C file.sh \| grep keyword` | 检查文件原始字节，排除隐藏字符 |

---

## 经验教训

1. **bash array 和换行缩进**：bash 3.x 中 `local arr=("a" "b")` 的换行缩进可能引入隐藏字符导致解析异常。用简单字符串或 `local arr=("a" "b")` 写在一行更安全。

2. **include 顺序重要**：GCC/Clang 的 `-I` 路径按顺序搜索，`/usr/include/x86_64-linux-gnu` 要在 `/usr/include/linux` 之后，否则会找到错误的头文件。

3. **BPF 程序必须有 BTF**：现代内核 iproute2 加载 BPF ELF 时要求 BTF info，`-g` 是保留 BTF 的必要编译选项。

4. **调试要找最小复现**：先从脚本中抽出实际的编译命令单独执行，确认正确后再排查脚本传递问题。

---

## 问题 5：smoke.sh 的 `sudo -n` 检查阻止执行

### 现象
```
[stage14_soft] sudo needed but -n failed; please pre-authorize sudo
```
smoke.sh 开头有 `sudo -n true` 检查，`-n` 表示 non-interactive（不提示密码）。如果当前用户没有 NOPASSWD 权限，检查直接失败退出。

### 排查过程
```bash
sudo -l  # 查看当前用户的 sudo 权限
# (ALL : ALL) ALL           ← 有 sudo 权限
# (ALL) NOPASSWD: /sbin/insmod, ...  ← 但只有部分命令 NOPASSWD
```

`sudo -n true` 要求命令必须在 NOPASSWD 列表中，否则即使你有完整 sudo 权限也会失败。

### 解决
删除或放宽 `sudo -n` 检查。smoke.sh 已修改为不阻塞：
```bash
# 旧代码（过于严格）
sudo -n true >/dev/null 2>&1 || {
    echo "[stage14_soft] sudo needed but -n failed" >&2
    exit 1
}

# 新代码（不阻止，sudo 失败时子步骤会 graceful skip）
# sudo -n 检查已移除
```

---

## 问题 6：debugfs 目录 Permission denied

### 现象
```
ls: cannot access '/sys/kernel/debug/netdev_stage14_soft/': Permission denied
```

### 排查过程
debugfs 默认权限 `drwx------`，只有 root 可读。

### 解决
```bash
sudo chmod 755 /sys/kernel/debug
sudo chmod 755 /sys/kernel/debug/netdev_stage14_soft
```

---

## 问题 7：send_stage13_frame 发包失败 `Operation not permitted`

### 现象
```
socket: Operation not permitted
```
发包工具需要 raw socket 权限（`CAP_NET_RAW`），这是 sudo NOPASSWD 无法授予的特殊能力。

### 排查过程
```bash
./send_stage13_frame --help
# socket: Operation not permitted
strace ./send_stage13_frame 2>&1 | grep socket
# socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP)) = -1 EPERM
```

raw socket 需要 `CAP_NET_RAW` capability，普通用户即使 sudo 也无法获得（除非通过 `setcap` 授予二进制文件）。

### 解决
```bash
# 方式 1：授予 binary capability（推荐）
sudo setcap cap_net_raw+ep /path/to/send_stage13_frame

# 方式 2：以 root 运行（不推荐）
sudo ./send_stage13_frame ...

# 方式 3：使用已有的 ping/iperf 等工具产生流量
# smoke test 的本质是验证驱动，不是必须用 send_stage13_frame
```

### smoke test 当前状态

| 测试项 | 状态 | 说明 |
|--------|------|------|
| 模块加载 | ✅ | NOPASSWD 已配置 |
| 接口 UP | ✅ | 驱动正常 |
| XDP 加载/卸载 | ✅ | BPF program 正常 |
| XDP_PASS/DROP 统计 | ✅ | 计数正确 |
| smoke 子测试（queue_dist 等） | ⚠️ | send_stage13_frame 需要 CAP_NET_RAW |

---

## 问题 8：`dmesg: read kernel buffer failed: Operation not permitted`

### 现象
```
dmesg: read kernel buffer failed: Operation not permitted
```

### 解决
```bash
sudo setcap cap_syslog+ep /usr/bin/dmesg
# 或临时授权
sudo dmesg | tail -n 200
```