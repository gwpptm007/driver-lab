# STAGE06_KNOWN_ISSUES

## 文档定位

这份文档把当前 stage06 已经真实遇到过的问题，整理成正式问题清单。  
目标不是追求“零问题”，而是保证：

- 现象可复现
- 根因可解释
- 修复方法可执行
- 后续复现时不再重复踩坑

---

## 问题 1：ARM64 kernel 缺网络核心符号

### 现象
构建模块时，`vmlinux.symvers` 中缺少：
- `register_netdev`
- `dev_add_pack`
- `free_netdev`

### 根因
目标 kernel `.config` 中：
- `CONFIG_NET` 未启用
- 或者相关网络核心未被重新编译并导出符号

### 修复
1. 在 `.config` 中启用：
   - `CONFIG_NET=y`
   - `CONFIG_NET_CORE=y`
2. 重新执行：
   - `make vmlinux`
   - `make Image`
   - `make modules`

### 复现时仍需注意
只改 `.config` 但不重建完整符号导出，仍可能继续缺符号。

---

## 问题 2：`CONFIG_PACKET` 缺失导致 smoke 失败

### 现象
运行 smoke 测试时报：
- `Address family not supported`

### 根因
未启用 AF_PACKET 所需内核配置。

### 修复
- 打开 `CONFIG_PACKET=m`
- 重新编译模块
- 把 `af_packet.ko` 放入目标 rootfs 并保证加载路径正确

### 复现时仍需注意
只打开配置但没有把模块打进 rootfs，运行时仍会失败。

---

## 问题 3：rootfs init shebang 错误

### 现象
系统启动时无法正确执行 `/init`，或者看起来像“文件存在但找不到”。

### 根因
使用了错误的 shebang，例如：
- `#!/busybox sh`

但实际 busybox 位于：
- `/bin/busybox`

### 修复
改成：
- `#!/bin/sh`

并确保：
- `/bin/sh -> busybox` 链接存在

### 复现时仍需注意
这是最小 rootfs 中非常典型的问题，尤其容易被误判成“QEMU 启动失败”。

---

## 问题 4：rootfs 缺少 `/proc` `/sys` 目录

### 现象
init 脚本中的：
- `mount -t proc none /proc`
- `mount -t sysfs none /sys`

失败。

### 根因
rootfs 打包前未创建对应目录。

### 修复
在打包前执行：
- `mkdir -p proc sys dev`

### 复现时仍需注意
即使脚本正确，目录缺失仍会让 mount 失败。

---

## 问题 5：平台脚本残留个人路径

### 现象
在作者机器上能跑，但换机器后解析到不存在的目录，或误指向旧产物。

### 根因
脚本内存在类似：
- `/home/wq7/...`
的 fallback

### 修复
统一改成：
1. 环境变量优先  
2. env 文件次之  
3. 仓库相对路径推导  
4. `command -v` 发现系统命令

### 复现时仍需注意
这类问题不会总是立刻失败，有时会“跑起来但引用了错误产物”，更危险。

---

## 问题 6：dry-run 通过但运行条件不满足

### 现象
dry-run 脚本能生成，但真正运行时失败。

### 根因
dry-run 只证明命令能被拼出来，不代表：
- kernel image 存在
- rootfs 可用
- 相关模块已打包
- QEMU 二进制存在

### 修复
把 dry-run 与 runtime smoke 分开验收：
- 先静态验证
- 再运行验证

### 复现时仍需注意
不要把 dry-run 成功等同于阶段运行通过。

---

## 问题 7：阶段边界容易被误解

### 现象
容易把 stage06 理解成“新驱动功能阶段”。

### 根因
看到 build / run / ARM64 smoke 后，容易误以为 stage06 也在增加主驱动新特性。

### 正确认知
stage06 的核心是：
- 平台迁移
- 兼容层
- 构建/运行/验收收口

不是：
- 多队列
- 新 transport
- 真实 NIC 深化建模

### 后续处理
把新功能扩展留到 stage07。

---

## 建议维护方式

后续每次遇到新问题，建议统一补充以下字段：

- 问题标题
- 触发条件
- 现象
- 根因
- 修复方法
- 是否仍需注意

这样 stage06 才能真正成为可复用阶段，而不是一次性实验记录。
