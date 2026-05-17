# app/ - fastpath-lite

`fastpath-lite` 是 `project-user-space-fastpath` 的核心 C 数据面程序，继承自 `l2fwd-lite` 并增加协议分类和 rewrite 功能。

## 1. 与 l2fwd-lite 的区别

| 特性 | l2fwd-lite | fastpath-lite |
|------|-------------|---------------|
| L2 MAC swap | ✅ | ✅ |
| 协议分类 | ❌ | ✅ ARP/IPv4/UDP/non_UDP |
| UDP-only 过滤 | ❌ | ✅ |
| MAC/IPv4/UDP rewrite | ❌ | ✅ |
| 精细化统计 | 基础 | arp/ipv4/udp/rewrite/drop |

## 2. 核心原理

### 2.1 协议分类流程

```
收到 mbuf
    ↓
解析 Ethernet type
    ↓
├─ ARP     → 交换 MAC → 转发
├─ IPv4/UDP → 按配置 rewrite 或交换 MAC → 转发
├─ IPv4/non-UDP → udp_only=1 时丢弃，否则交换 MAC → 转发
└─ 其他    → 丢弃或交换 MAC
```

### 2.2 rewrite 规则

当 `--rewrite 1` 启用时，可以指定：

```bash
--rewrite-src-mac 02:00:00:00:00:11   # 替换 src MAC
--rewrite-dst-mac 02:00:00:00:00:22   # 替换 dst MAC
--rewrite-src-ip 10.10.1.10           # 替换 src IP
--rewrite-dst-ip 10.10.2.20           # 替换 dst IP
--rewrite-src-port 5000              # 替换 src UDP port
--rewrite-dst-port 6000               # 替换 dst UDP port
```

rewrite 会自动重新计算 IPv4 checksum。

### 2.3 UDP-only 模式

```bash
--udp-only 1
```

启用后，只允许 UDP 包通过，非 UDP 包（包括 IPv4 non-UDP、ARP 等）会被丢弃。

## 3. 关键代码结构

```c
int main(int argc, char **argv)
{
    rte_eal_init(argc, argv);           // 1. EAL 初始化
    parse_app_args(argc, argv);           // 2. 解析参数（支持 rewrite 等）
    mbuf_pool = rte_pktmbuf_pool_create(); // 3. 创建 mbuf 池
    init_all_ports(mbuf_pool);           // 4. 初始化端口
    forwarding_loop();                    // 5. 转发循环（调用 classify_and_rewrite）
    print_sw_stats();                     // 6. 打印统计
    print_ethdev_stats();                // 7. 打印硬件统计
    stop_all_ports();                     // 8. 停止端口
    rte_eal_cleanup();                    // 9. 清理
}
```

### 核心分类函数

```c
classify_and_rewrite(src_portid, mbuf)
    ├─ 解析 Ethernet type
    ├─ ARP:     swap_mac → true
    ├─ IPv4:    handle_ipv4_udp() → 处理 UDP rewrite
    └─ Other:   udp_only? drop : swap_mac
```

## 4. 软件统计详解

```text
rx_packets       - 收到的包总数
tx_packets       - 发送的包总数
tx_failed        - 发送失败（TX 队列满）
arp_packets      - ARP 包数
ipv4_packets     - IPv4 包数
udp_packets      - UDP 包数
non_udp_packets  - 非 UDP 包数
rewrite_packets  - 被 rewrite 的包数
drop_short       - 包太短被丢弃
drop_non_udp    - udp_only 模式下非 UDP 被丢弃
drop_no_peer     - 无配对端口被丢弃
```

## 5. 构建系统详解

### 5.1 目录结构

```
app/
├── meson.build       # Meson 构建配置（告诉构建系统：要编译什么、依赖什么）
├── main.c            # 源代码
├── Makefile         # 封装 meson + ninja 的便捷构建入口
└── build/          # 编译产物目录（meson setup 后自动创建）
    ├── compile_commands.json
    └── fastpath-lite  # 最终可执行文件
```

### 5.2 构建方式

#### 方式一：使用 Makefile（推荐）

```bash
cd app

# 清理并重新编译
make rebuild

# 或分步执行
make clean    # 删除 build 目录
make all     # 配置 + 编译
```

#### 方式二：手动执行 meson + ninja

```bash
cd app

# 1. 配置（生成 build/ 目录和 ninja 文件）
meson setup build

# 2. 编译
ninja -C build

# 3. 查看可执行文件
ls -lh build/fastpath-lite
```

### 5.3 Makefile 解析

```makefile
APP := fastpath-lite              # 项目名
BUILD_DIR := build                 # 编译产物目录

.PHONY: all clean rebuild         # 声明伪目标（不是真实文件）

all:
    # meson setup: 配置项目，检测 libdpdk 依赖
    # --wipe: 如果 build 已存在，清除旧配置重新配置
    # || meson setup: 备用命令（第一次 build 不存在时自动用这个）
    meson setup $(BUILD_DIR) --wipe || meson setup $(BUILD_DIR)
    ninja -C $(BUILD_DIR)          # 用 ninja 编译

clean:
    rm -rf $(BUILD_DIR)           # 删除编译产物

rebuild: clean all                # rebuild = clean + all
```

### 5.4 Meson 构建系统

**为什么用 Meson？**

- DPDK 官方推荐的构建方式
- 比传统 Makefile 更快、更易用
- 自动检测依赖（pkg-config）

**Meson 核心概念：**

| 概念 | 说明 |
|------|------|
| `meson.build` | 构建配置文件，描述项目结构、依赖、源文件 |
| `meson setup build` | 配置阶段，在 build/ 下生成 ninja 能读的文件 |
| `ninja -C build` | 编译阶段，ninja 读取配置并编译 |

**Meson 的工作流程：**

```
meson.build  ──meson setup──►  build/  ──ninja──►  fastpath-lite
 (源码)        (生成构建文件)      (编译产物)
```

### 5.5 meson.build 解析

```meson
project('fastpath-lite', 'c', default_options: ['warning_level=2'])
#  project()       定义项目名
#  'c'             使用 C 语言
#  warning_level=2  编译器警告级别

dpdk_dep = dependency('libdpdk')
#  dependency()    通过 pkg-config 查找依赖
#  libdpdk         DPDK 的 pkg-config 名称

executable('fastpath-lite',
  'main.c',                    # 源文件列表
  dependencies: dpdk_dep,      # 依赖 libdpdk
  install: false)              # 不安装到系统路径
```

### 5.6 Makefile vs Meson 对比

| 操作 | 传统 Makefile | Meson + Ninja |
|------|--------------|---------------|
| 配置 | `make` 或手动 | `meson setup build` |
| 编译 | `make` | `ninja -C build` |
| 清理 | `make clean` | `rm -rf build` |
| 依赖检测 | 手动写规则 | 自动通过 pkg-config |

### 5.7 常见问题

**Q：编译失败怎么办？**

```bash
# 1. 删除 build 目录，重新配置
rm -rf build
meson setup build
ninja -C build

# 2. 使用 Makefile 清理重建
make rebuild
```

**Q：如何检查依赖是否安装？**

```bash
pkg-config --modversion libdpdk
# 应该显示 DPDK 版本，如 21.11.9
```

**Q：修改代码后如何重新编译？**

```bash
# 直接重新编译即可，ninja 会自动检测哪些文件变了
ninja -C build

# 或用 Makefile
make all
```

### 5.8 构建产物

编译成功后：

```
app/build/fastpath-lite      # 可执行文件
app/build/compile_commands.json  # 编译命令记录（供 IDE 使用）
```

## 6. 运行示例

### 单端口 smoke

```bash
sudo ./build/fastpath-lite -l 0-1 -n 4 \
  --file-prefix fastpath_lite \
  -a 0000:0b:00.0 \
  -- \
  --run-seconds 20 --stats-period 2
```

### UDP-only 过滤

```bash
sudo ./build/fastpath-lite -l 0-1 -n 4 \
  -a 0000:0b:00.0 \
  -- \
  --udp-only 1 --run-seconds 20
```

### rewrite 示例

```bash
sudo ./build/fastpath-lite -l 0-1 -n 4 \
  -a 0000:0b:00.0 \
  -- \
  --rewrite 1 \
  --rewrite-src-ip 10.10.1.10 \
  --rewrite-dst-ip 10.10.2.20 \
  --rewrite-src-port 5000 \
  --rewrite-dst-port 6000 \
  --run-seconds 20
```

## 7. 下一步演进方向

```
当前: fastpath-lite (分类 + rewrite)
  ↓
未来: user-space-fastpath (生产级)
  ├─ per-flow stats (流级统计)
  ├─ control-plane config (控制面配置)
  ├─ records/replay/report (流量回放)
  ├─ flow table / ACL
  └─ multi-lcore scaling (多核扩展)
```