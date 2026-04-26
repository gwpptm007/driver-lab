# 06_OBSERVE_AND_VALIDATE

## 这一步不要求大实验，但建议做最小验证

### 最小验证项

1. 确认接口驱动
```bash
ethtool -i <ifname>
```

2. 确认 PCI 设备
```bash
lspci -nnk | grep -A 3 -i ethernet
```

3. 导出基础 stats
```bash
ethtool -S <ifname>
ip -s link show <ifname>
```

## 为什么要做这几步

因为这个 Lab 虽然以源码对照为主，但如果完全没有运行期环境感，  
后面的差异总结会太虚。

## 当前目标

- 不追求重压测
- 只要把：
  - 设备
  - 驱动
  - stats
  - 基础运行环境
对上即可
