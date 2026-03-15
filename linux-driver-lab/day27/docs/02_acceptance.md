# day27 验收清单

## 必须满足

- [ ] 200 次循环通过
- [ ] 无 oops、无 hung task、无明显内存泄漏迹象
- [ ] `records/` 中有循环摘要

## 建议额外补充

- [ ] 关键命令已写入 README 或 output
- [ ] 失败路径至少记录一个例子
- [ ] 下一天的输入材料已经准备好

## 不通过时优先排查

- IRQ/vector、iomap、chrdev、kthread 清理遗漏
- 模块退出时仍有打开文件或等待队列
