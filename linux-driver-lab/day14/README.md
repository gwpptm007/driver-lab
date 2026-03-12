# Day14 - bring-up checklist（1页版）

## 目标
拿到一份新的寄存器表后，不是立刻写驱动，而是按一张 **可复用的 bring-up checklist** 推进联调：先确认“能不能安全访问”，再确认“初始化顺序”，最后再扩展到 IRQ / DMA / 数据路径。

> 这份清单适合 day08-day13 这条学习主线往后延伸，也适合以后拿真实芯片手册、IP 寄存器表、FPGA 寄存器表时复用。

---

## 输入物（先收齐）

- [ ] 寄存器表/芯片手册：基地址、偏移、位定义、复位值、访问属性（RO/RW/W1C 等）
- [ ] 上电/时钟/reset/电源域说明
- [ ] 中断说明：中断号、触发类型、状态位、清中断方式
- [ ] 数据路径说明：PIO / FIFO / DMA / doorbell / descriptor 等
- [ ] Linux 侧资源信息：DT `reg` / `interrupts` / `clocks` / `resets` / `power-domains`
- [ ] 最小验收目标：先跑通“读版本寄存器”还是“中断计数”还是“收发一包数据”

---

## bring-up 推进节奏（核心清单）

| 阶段 | 要确认什么 | 最低产出 |
|---|---|---|
| 1. 先读表，不急写代码 | 基地址是否唯一；寄存器是否按 block 分组；哪些寄存器上电后可直接读；哪些寄存器有写保护/解锁顺序 | 一页“寄存器地图”草图：控制、状态、中断、数据、调试寄存器分组 |
| 2. 先跑最小访问路径 | 只做 `probe -> ioremap/regmap -> 读版本/ID/状态寄存器`；不要一上来开中断、开 DMA | 日志里能打印：基地址、版本号、默认状态值 |
| 3. 先确认依赖条件 | clock/reset/电源域/pinctrl 是否已经 ready；不满足时读出来可能全 0、全 1 或总线异常 | README 里写清楚“访问寄存器前的前置条件” |
| 4. 明确访问语义 | 哪些位是 RW；哪些是 W1C；哪些字段必须 read-modify-write；哪些寄存器读会清状态 | 一张“危险寄存器清单” |
| 5. 先做只读观测，再做写入 | 第一次联调优先读 `ID/VERSION/STATUS`；写入时先写最安全的软配置位，不先碰关键 enable 位 | 第一个可复现测试命令和期望日志 |
| 6. 初始化顺序单独列出来 | reset deassert -> clock enable -> 清 pending -> 配门限/掩码 -> enable 模块/中断；顺序错了常导致假死 | README 中一段“推荐初始化时序” |
| 7. 中断联调只看一条最短链 | `status -> mask -> ack/clear -> handler`；先确认中断来源、清中断方式，再谈 bottom-half | 能解释 `/proc/interrupts` 或 trace 中看到的 IRQ 路径 |
| 8. 数据路径最后做 | FIFO 深度、门限、descriptor ownership、cache coherency、DMA mask 这些通常比寄存器读写更晚验证 | 单独的 data path TODO，不和最小 bring-up 混在一起 |
| 9. 每次联调都留证据 | 保存 dmesg、寄存器快照、trace、示波器/逻辑分析仪截图（如有） | 一个可回放的问题记录目录 |

---

## 联调时最常见的 8 个卡点

1. **基地址错**：DT `reg` 看起来对，但实际 IP 映射窗口不对。  
2. **依赖没开**：clock/reset/power-domain 没 ready，导致寄存器读值异常。  
3. **访问语义理解错**：把 W1C 当普通 RW 写，越调越乱。  
4. **初始化顺序错**：还没 clear pending 就 enable interrupt。  
5. **状态位没有真正清掉**：write 了，但缺少 read-back 或写错 clear 位。  
6. **跨层信息不一致**：手册、FPGA 文档、驱动头文件、DT 描述互相不一致。  
7. **一次想做太多**：把 probe、IRQ、DMA、数据面一起上，定位困难。  
8. **没有证据链**：只说“寄存器不对”，但没有 dmesg / dump / trace 截图。  

---

## 最小可复用模板（推荐记法）

### 第 1 轮：只证明“能安全访问”
- [ ] 资源解析成功
- [ ] `ioremap` 或 `regmap` 初始化成功
- [ ] 读到 `ID/VERSION`
- [ ] 读到 `STATUS`
- [ ] 导出一份 debugfs/proc 寄存器快照

### 第 2 轮：只证明“初始化顺序正确”
- [ ] clock/reset/power-domain 顺序写清楚
- [ ] 关键控制寄存器写入后 read-back 正常
- [ ] 中断状态位可置位、可清除

### 第 3 轮：只证明“一条功能链路成立”
- [ ] 一次 IRQ 路径可解释
- [ ] 或一笔最小 PIO/FIFO 数据可走通
- [ ] 或一次最小 DMA 描述符能完成

---

## 一句话原则
**拿到寄存器表后，先做“最小安全访问 + 初始化顺序确认 + 证据留存”，再做 IRQ、DMA 和完整数据路径。不要一上来把所有功能一起 bring-up。**
