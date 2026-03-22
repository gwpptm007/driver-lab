# day24 结果与验收说明

## 1. day24 要证明什么

day24 的目标不是停留在“驱动已经接住设备”，而是进一步证明：

- 驱动已经把 `ivshmem` 的 BAR0/BAR2 安全映射起来
- BAR2 起始区域上的最小协议头已经初始化
- 用户态可以通过字符设备 `/dev/day24_ivshmem0`：
  - 读出设备信息与协议头
  - 用 `mmio-write` 修改协议头字段
  - 再用 `mmio-read` 读回写入值
  - 用 `shm-write` 写入 payload
  - 再用 `shm-read` 原样读回 payload
- 最后能安全 `rmmod`

## 2. 本次真实测试结论

**day24 通过。**

你上传的 `records/day24-local-001/` 已经形成一条完整证据链：

- `run-summary.md` 中 9 项全是 `yes`
- `mmio-info.txt` 已经读到正确的 vendor/device、BAR 信息和协议头
- `mmio-read-before.txt → mmio-write-state.txt → mmio-read-after.txt` 证明了协议头字段写前/写后闭环
- `shm-write.txt → shm-read.txt` 证明了 payload 写入/读回闭环
- `dmesg-driver.txt` 证明驱动在 `probe()` 中完成了 BAR 映射、协议头初始化，并记录了后续写操作
- `lspci-nn.txt` / `lspci-vv-nn.txt` 证明 guest 内看到的设备和资源布局与驱动日志相互印证
- `qemu.stderr.log` 为空，说明本轮没有额外的 QEMU 运行时错误

## 3. 如何逐项解释输出文件

### 3.1 `run-summary.md`

这是第一层总览：

- `insmod 成功：yes`
- `probe 成功：yes`
- `mmio info：yes`
- `mmio write：yes`
- `mmio read after：yes`
- `shm write：yes`
- `shm read：yes`
- `rmmod 成功：yes`
- `guest 流程完成：yes`

这说明自动流程的关键步骤都已跑到成功状态。

### 3.2 `mmio-info.txt`

这是第二层核心证据，证明驱动内部状态是正确的：

- `vendor=0x1af4 device=0x1110`
  - 证明 day24 驱动接住的设备就是 ivshmem
- `bar0_first_dword=0x00000000`
  - 证明 BAR0 已完成映射并能进行安全只读访问
- `proto_magic=0x44593234`
  - 这是驱动初始化协议头时写入的魔数（`DY24`）
- `proto_version=1`
  - 证明协议版本字段初始化成功
- `seq=0 state=1 payload_len=0`
  - 证明协议头初始状态正确
- `BAR0 ... len=0x100`
- `BAR2 ... len=0x400000`
  - 证明驱动识别到的资源范围符合预期

### 3.3 `mmio-read-before.txt`

内容是：

- `offset=0x0000000c value=0x00000001`

这里 `0x0c` 对应协议头中的 `state` 字段。初始值为 `1`，说明协议头初始化后的状态值正确。

### 3.4 `mmio-write-state.txt`

这个文件同时包含两层证据：

- dmesg 日志：
  - `MMIO write: offset=0x0000000c value=0x00000003 seq=1`
- 用户态工具返回：
  - `mmio-write ok: offset=0x0000000c value=0x00000003`

这说明：

1. 用户态确实发起了一次受控的 MMIO 写
2. 驱动确实把 `state` 从初始值 `1` 更新为 `3`
3. 同时 `seq` 被推进到 `1`

### 3.5 `mmio-read-after.txt`

内容是：

- `offset=0x0000000c value=0x00000003`

它和前面的 `mmio-write-state.txt` 形成闭环：

- 写前 `state = 1`
- 写后 `state = 3`
- 再读一次，读回的还是 `3`

这就证明 day24 的 **MMIO 头字段读写闭环成立**。

### 3.6 `shm-write.txt`

这个文件同样包含内核和用户态两层证据：

- dmesg：
  - `payload write: count=19 new_len=19 seq=2`
- 用户态：
  - `shm-write ok: wrote=19 text=hello_day24_ivshmem`

这说明：

1. 用户态把 19 字节 payload 写入了 BAR2 共享内存窗口
2. 驱动把 `payload_len` 更新成了 19
3. `seq` 进一步推进到 2

### 3.7 `shm-read.txt`

内容是：

- `shm-read ok: read=19 text=hello_day24_ivshmem`

这和 `shm-write.txt` 形成第二个完整闭环：

- 写入字符串：`hello_day24_ivshmem`
- 读回字符串：`hello_day24_ivshmem`
- 长度仍然是 19

这说明 day24 的 **BAR2 payload 写入/读回闭环成立**。

### 3.8 `dmesg-driver.txt`

这是最关键的内核侧证据，按时间顺序证明了 day24 驱动内部流程：

1. PCI 枚举阶段：
   - `pci 0000:00:02.0: [1af4:1110]`
   - `BAR 0 assigned`
   - `BAR 2 assigned`
2. 驱动加载后：
   - `probe enter`
   - `BAR0 ...`
   - `BAR2 ...`
   - `BAR0 mapped`
   - `BAR2 mapped`
   - `BAR0 first dword=0x00000000`
   - `protocol header initialized: magic=0x44593234 version=1 payload_cap=256`
   - `probe success: device=day24_ivshmem0 payload_cap=256`
3. 运行过程中：
   - `MMIO write ... value=0x00000003 seq=1`
   - `payload write: count=19 new_len=19 seq=2`

这证明 day24 驱动不只是“插上了”，而是已经真正完成：

- BAR 映射
- 协议头初始化
- 头字段写入
- payload 写入

### 3.9 `lspci-nn.txt` 与 `lspci-vv-nn.txt`

这两份文件是用户态视角下的设备证据。

`lspci-nn.txt` 证明：

- `00:02.0 Class [0500]: Device [1af4:1110] (rev 01)`

`lspci-vv-nn.txt` 进一步证明：

- `Region 0: Memory at 10081000 ... [size=256]`
- `Region 2: Memory at 8000000000 ... [size=4M]`

这和驱动日志里的 BAR0/BAR2 打印是一一对应的，说明：

- 设备资源识别是对的
- 驱动拿到的 BAR 信息不是“拍脑袋写的”，而是和 PCI 枚举结果吻合

### 3.10 `qemu.stderr.log`

当前为空。

这意味着本轮没有额外的 QEMU 启动或运行时错误，是一个正向信号。

## 4. 用这些输出如何判定“通过”

只要满足下面这些，就可以判 day24 通过：

1. `run-summary.md` 关键项全为 `yes`
2. `mmio-info.txt` 能读出：
   - 正确的 `vendor/device`
   - 正确的 `magic/version/state`
   - 正确的 BAR0/BAR2 资源信息
3. `mmio-read-before.txt` 与 `mmio-read-after.txt` 能体现出：
   - `state` 从 `1` 变成 `3`
4. `shm-write.txt` 与 `shm-read.txt` 能体现出：
   - 写入的 payload 被原样读回
5. `dmesg-driver.txt` 中存在：
   - `probe success`
   - `MMIO write`
   - `payload write`
6. `lspci-vv-nn.txt` 中存在 BAR0/BAR2 的 Region 信息
7. `serial.log` 中应出现 `===DAY24:COMPLETE===`

## 5. 当前这轮结果为什么可以直接判通过

因为本轮输出已经同时证明了两件最关键的事：

### 5.1 协议头字段的 MMIO 读写闭环成立

- 写前：`state=1`
- 写入：`state=3`
- 读后：`state=3`

### 5.2 BAR2 payload 的共享内存读写闭环成立

- 写入：`hello_day24_ivshmem`
- 读回：`hello_day24_ivshmem`
- 长度一致，内容一致

因此，day24 的目标“MMIO 读写 + 共享内存协议闭环”已经被真实输出证明达成。
