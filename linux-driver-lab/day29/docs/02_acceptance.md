# day29 验收清单

## 必须满足

- [ ] DMA buffer 分配成功
- [ ] 数据一致性校验通过
- [ ] 无 crash、无 DMA mapping error

## 建议额外补充

- [ ] 关键命令已写入 README 或 output
- [ ] 失败路径至少记录一个例子
- [ ] 下一天的输入材料已经准备好

## 不通过时优先排查

- 没区分 coherent buffer 与 streaming DMA
- 错误地把虚拟地址直接写给设备
