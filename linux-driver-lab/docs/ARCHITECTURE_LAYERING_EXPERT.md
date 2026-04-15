# 架构分层图（专家评审口径）

## 1. 项目总体分层

```mermaid
flowchart TD
    A[driver-lab] --> B[kernel-src]
    A --> C[linux-driver-lab]

    B --> B1[linux-5.15.10 source/build/output]
    B --> B2[busybox-1.36.1 source/output]

    C --> C1[foundation day01~day35]
    C --> C2[netdev stage00~stage06]
    C --> C3[docs / roadmap / progress]

    C1 --> C11[W1 char-device basics]
    C1 --> C12[W2 platform DT IRQ regmap]
    C1 --> C13[W3 baseline perf regression]
    C1 --> C14[W4 PCIe BAR MMIO MSI]
    C1 --> C15[W5 DMA mmap bench trace stability]

    C2 --> C21[stage00 bootstrap]
    C2 --> C22[stage01 net_device skeleton]
    C2 --> C23[stage02 skb path]
    C2 --> C24[stage03 NAPI poll]
    C2 --> C25[stage04 ring DMA replenish]
    C2 --> C26[stage05 virtio comparison]
    C2 --> C27[stage06 ARM64 migration]
```

---

## 2. Foundation 与 Netdev 的关系

```mermaid
flowchart LR
    A[Foundation] --> A1[char-device / miscdevice]
    A --> A2[platform_driver / DT / IRQ]
    A --> A3[PCIe / BAR / MSI]
    A --> A4[DMA / mmap / perf / stability]

    A4 --> B[Netdev]
    B --> B1[net_device]
    B --> B2[sk_buff]
    B --> B3[NAPI]
    B --> B4[ring descriptor]
    B --> B5[RX replenishment]
    B --> B6[virtio-net compare]
    B --> B7[ARM64 migration]
```

解释：

- Netdev 不是重新换题
- 它建立在前面 IRQ、DMA、QEMU、工程化脚本能力之上
- 因此当前项目是连续演进，而不是两套无关目录

---

## 3. 第二阶段内部演进关系

```mermaid
flowchart LR
    S0[stage00 bootstrap] --> S1[stage01 skeleton]
    S1 --> S2[stage02 skb loop]
    S2 --> S3[stage03 NAPI model]
    S3 --> S4[stage04 ring + DMA]
    S4 --> S5[stage05 virtio source anchor + platform param]
    S5 --> S6[stage06 ARM64 migration + cross-platform closure]
```

---

## 4. stage04 的数据路径抽象

```mermaid
flowchart LR
    U[userspace sender] --> T[xmit]
    T --> M1[dma_map_single TX]
    M1 --> R1[find RX posted slot]
    R1 --> C[memcpy simulate device DMA]
    C --> D[mark desc DONE + CPU owner]
    D --> I[raise irq]
    I --> N[napi_schedule]
    N --> P[poll budget loop]
    P --> M2[dma_unmap_single RX]
    M2 --> S[skb_put + eth_type_trans]
    S --> K[netif_receive_skb]
    K --> F[refill RX slot]
```

这张图对应的专家判断是：

- stage04 已经把“网卡驱动核心组织方式”具象化
- 虽然仍是教学型模型，但关键语义已经对了

---

## 5. 评审视角下的三层能力结构

```mermaid
flowchart TD
    A[作品层] --> A1[README / review / roadmap / acceptance]
    A --> A2[records / output / reports]

    B[工程层] --> B1[env / scripts / build / smoke / collect]
    B --> B2[platform matrix / diff / migration]

    C[代码层] --> C1[driver core logic]
    C --> C2[tools / guest / userspace helpers]
    C --> C3[kcompat / headers / Makefile glue]
```

结论：

- 这个仓库已经不是“只有代码层”
- 作品层、工程层、代码层三者都已经存在
- 专家评审时，这种三层都完整的仓库会明显更有说服力
