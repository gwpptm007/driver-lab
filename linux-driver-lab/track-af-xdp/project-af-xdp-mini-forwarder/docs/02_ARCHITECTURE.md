# 02_ARCHITECTURE

## 数据路径

```text
NIC RX queue
   |
   v
XDP program
   |
   |-- no xsks_map[queue] -> XDP_PASS
   |
   `-- xsks_map[queue]    -> bpf_redirect_map()
                                 |
                                 v
                         AF_XDP RX ring
                                 |
                                 v
                         user-space loop
                          |              |
                          | drop         | reflect
                          v              v
                    FILL ring       TX ring -> COMPLETION ring -> FILL ring
```

## 模块说明

| 文件 | 作用 |
|---|---|
| `af_xdp_forwarder_kern.bpf.c` | XDP 侧 XSKMAP redirect |
| `af_xdp_forwarder.c` | 用户态 UMEM / XSK / ring / stats / drop/reflect |
| `scripts/*.sh` | 环境检查、编译、运行、收集、review |
| `tools/parse_forwarder_stats.py` | 从日志提取最终 stats |

## 为什么先做 reflect

真实双口 forward 需要至少两个 RX/TX 队列或两张网卡。当前 VMware 测试机环境主要是单个 `ens192/vmxnet3` 测试口，所以先做 `reflect`：

- 能验证 RX ring；
- 能验证 TX ring；
- 能验证 COMPLETION ring；
- 能验证 frame 所有权流转；
- 不需要第二张物理网卡。
