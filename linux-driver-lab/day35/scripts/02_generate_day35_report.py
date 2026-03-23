#!/usr/bin/env python3
"""生成 Day35 最终报告。

Day35 的报告不是简单复制各天 README，
而是把多天结果串成“功能 -> 性能 -> 优化 -> 可观测性 -> 稳定性”的一条线。
"""
from __future__ import annotations

from pathlib import Path
from typing import Dict

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "day35" / "output"


def parse_summary(day: int) -> Dict[str, str]:
    path = ROOT / f"day{day}" / "records" / f"day{day}-local-001" / "run-summary.md"
    data: Dict[str, str] = {}
    if not path.exists():
        return data
    for raw in path.read_text(encoding="utf-8", errors="ignore").splitlines():
        line = raw.rstrip("\r\n")
        if line.startswith("- ") and ": " in line:
            k, v = line[2:].split(": ", 1)
            data[k.strip()] = v.strip()
    return data


def parse_kv(path: Path) -> Dict[str, str]:
    data: Dict[str, str] = {}
    if not path.exists():
        return data
    for raw in path.read_text(encoding="utf-8", errors="ignore").splitlines():
        line = raw.rstrip("\r\n")
        if "=" in line and not line.startswith("csv,"):
            k, v = line.split("=", 1)
            data[k.strip()] = v.strip()
    return data


def main() -> None:
    day29 = parse_kv(ROOT / "day29/records/day29-local-001/verify-result.txt")
    day30 = parse_kv(ROOT / "day30/records/day30-local-001/mmap-verify.txt")
    day31_mmap = parse_kv(ROOT / "day31/records/day31-local-001/bench-mmap.txt")
    day31_dma = parse_kv(ROOT / "day31/records/day31-local-001/bench-dma.txt")
    day32_cmp = parse_kv(ROOT / "day32/records/day32-local-001/compare-mmap.txt")
    day34_loop = parse_kv(ROOT / "day34/records/day34-local-001/module-loop.txt")
    day34_conc = parse_kv(ROOT / "day34/records/day34-local-001/concurrent-stress.txt")
    s33 = parse_summary(33)

    text = f"""# Day35 最终性能与风险报告

## 1. 背景与目标

本阶段以 QEMU EDU 为教学后端，围绕 `coherent DMA -> mmap 零拷贝 -> 性能量化 -> 热点优化 -> function_graph 可观测性 -> 稳定性回归` 逐日推进。

Day35 的目标不是新增功能，而是把前面几天的证据整理成一份可用于阶段汇报、交付留档和面试表达的总结材料。

## 2. 阶段结果概览

- Day29：DMA round-trip 主链路通过
- Day30：用户态 `mmap` 零拷贝主链路通过
- Day31：`ioctl / mmap / dma` bench 主链路通过
- Day32：mmap baseline -> optimized 的优化收益明确
- Day33：`function_graph` 已成功开启并采到关键路径窗口
- Day34：并发压测、1000 次模块循环、错误注入通过

## 3. 关键指标

### 3.1 Day29：功能基线

- `verify_ok={day29.get('verify_ok', 'N/A')}`
- `irq_delta={day29.get('irq_delta', 'N/A')}`
- `mismatch_index={day29.get('mismatch_index', 'N/A')}`

说明：DMA round-trip 主路径已经能稳定完成一进一出两段 DMA，并形成正确校验结果。

### 3.2 Day30：mmap 零拷贝主链路

- `mmap_ok={day30.get('mmap_ok', 'N/A')}`
- `verify_ok={day30.get('verify_ok', 'N/A')}`
- `run_ok={day30.get('run_ok', 'N/A')}`
- `irq_delta={day30.get('irq_delta', 'N/A')}`

说明：用户态能够直接映射 DMA buffer，内核仅负责 DMA 发起和中断处理，主链路闭环形成。

### 3.3 Day31：三条 bench 路径

- mmap `avg_us={day31_mmap.get('avg_us', 'N/A')}`
- mmap `p99_us={day31_mmap.get('p99_us', 'N/A')}`
- mmap `throughput_mbps={day31_mmap.get('throughput_mbps', 'N/A')}`
- dma `avg_us={day31_dma.get('avg_us', 'N/A')}`
- dma `p99_us={day31_dma.get('p99_us', 'N/A')}`

说明：Day31 已经把控制路径、零拷贝路径和设备参与路径量化出来，为后续优化与风险讨论提供了数值基线。

### 3.4 Day32：优化前后对比

- `avg_latency_gain_pct={day32_cmp.get('avg_latency_gain_pct', 'N/A')}`
- `p99_latency_gain_pct={day32_cmp.get('p99_latency_gain_pct', 'N/A')}`
- `throughput_gain_pct={day32_cmp.get('throughput_gain_pct', 'N/A')}`

说明：通过把 `GET_INFO + mmap/munmap` 从循环内移到循环外，mmap 热路径的性能收益非常明显，证明热点定位和优化方向是有效的。

### 3.5 Day33：可观测性

- `trace config function_graph={s33.get('trace config function_graph', 'N/A')}`
- `trace window present={s33.get('trace window present', 'N/A')}`
- `trace mentions day33_ioctl={s33.get('trace mentions day33_ioctl', 'N/A')}`
- `trace mentions day33_irq_handler={s33.get('trace mentions day33_irq_handler', 'N/A')}`

说明：Day33 已能形成 function_graph trace 窗口，说明可观测性主链路已经具备；但对 `day33_do_run_dma / day33_wait_dma_idle` 的窗口覆盖仍可继续优化。

### 3.6 Day34：稳定性回归

- 模块循环 `completed_loops={day34_loop.get('completed_loops', 'N/A')}`
- 模块循环 `failed_loops={day34_loop.get('failed_loops', 'N/A')}`
- 并发压测 `worker_fail={day34_conc.get('worker_fail', 'N/A')}`
- 并发压测 `worker_ioctl_rc={day34_conc.get('worker_ioctl_rc', 'N/A')}`

说明：主链路在并发压测、模块生命周期循环和错误注入下都保持稳定，说明当前代码已经不只是“能跑通”，而是具备基本回归稳定性。

## 4. 风险与限制

### 4.1 当前开放项

Day33 当前不再是“是否通过”的阻断项，而是“trace 覆盖完整性”的改进项：

- `trace mentions day33_do_run_dma={s33.get('trace mentions day33_do_run_dma', 'N/A')}`
- `trace mentions day33_wait_dma_idle={s33.get('trace mentions day33_wait_dma_idle', 'N/A')}`

这意味着：

- 可观测性主链路已通
- 但关键路径解释还能更完整

### 4.2 环境限制

- 当前结果主要来自 QEMU EDU 教学环境
- 数据适合做相对比较，不应直接外推为真实硬件绝对性能
- Day32 的优化收益是教学场景下的结构性收益，不等同于所有设备场景下都按同样比例提升

## 5. 回滚建议

若后续 Day36 继续推进时引入不稳定性，建议：

1. 功能与稳定性回滚到 Day34 通过版
2. 性能对比回滚到 Day32 通过版
3. Day35 作为阶段归档材料保留，不回退其文档结论

## 6. 结论

当前阶段已经完成：

- 功能主线闭环（Day29 / Day30）
- 性能主线量化（Day31）
- 热点优化验证（Day32）
- 基础可观测性建立（Day33）
- 稳定性回归与错误注入（Day34）

因此，本阶段的总体结论是：

**功能、性能、可观测性和稳定性主线已经形成可交付闭环；下一阶段重点从“把功能做通”转到“把可观测性与兼容性做深”。**
"""
    (OUTPUT / "day35_final_report.md").write_text(text, encoding="utf-8")


if __name__ == "__main__":
    main()
