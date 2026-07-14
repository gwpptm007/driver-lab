# eBPF 视觉学习试点测试记录（2026-07-14）

## 1. 测试目标

验证 fundamentals 00-02 的混合视觉学习层满足以下要求：

1. 每章同时提供 Mermaid、静态 PNG、动态 GIF 和可单步操作的 Canvas。
2. 视觉节点能映射到真实内核对象、执行上下文、源码锚点、风险与验证命令。
3. 资产可由 Chrome/Edge + FFmpeg 重复生成，而不是手工维护截图。
4. 桌面 1200x720 与移动端 390x844 不出现空白画布、黑块、标题/控件重叠或横向裁切。
5. 文档改造不破坏 135 上已有 eBPF clean build 与 loopback runtime smoke。

## 2. 环境

| 项目 | 值 |
|---|---|
| 本地开发机 | Windows / PowerShell 5.1 |
| 浏览器 | Google Chrome headless（脚本也支持 Microsoft Edge fallback） |
| GIF 工具 | FFmpeg |
| 远端测试机 | `192.168.65.135`，用户 `wq7` |
| 远端内核 | `6.8.0-124-generic` |
| 远端隔离目录 | `/tmp/ebpf-visual-pilot-20260714-234624` |

远端测试使用临时归档，不覆盖 `/home/wq7/workspace` 下的长期工作区。测试命令中的密码统一写为占位符，不在记录中保存凭据。

## 3. TDD 与审计过程

### 3.1 初始 RED

在创建资产前运行：

```powershell
py tests/check_visual_assets.py
```

初始 marker：

```text
EBPF_VISUAL_ASSET_AUDIT_FAIL errors=22
```

缺项覆盖共享 CSS/JS、3 个 Canvas 页面、3 组 PNG/GIF，以及 00-02 文档引用。

### 3.2 多余资产审计 RED

渲染器早期参数错误曾生成匿名 `.png/.gif`。先扩展测试，要求资产目录只能包含已声明文件：

```text
EBPF_VISUAL_ASSET_AUDIT_FAIL errors=1
ERROR: unexpected visual assets: .gif, .png
```

删除残留后转绿，最终资产目录只有 6 个声明文件。

## 4. 可重复渲染

### 4.1 完整命令

```powershell
cd linux-driver-lab/track-ebpf-observability
powershell -ExecutionPolicy Bypass -File `
  docs/fundamentals/visuals/tools/render_visuals.ps1
```

### 4.2 单场景命令

```powershell
powershell -ExecutionPolicy Bypass -File `
  docs/fundamentals/visuals/tools/render_visuals.ps1 `
  -Scene 01_ebpf_object_lifecycle
```

最终 marker：

```text
EBPF_VISUAL_SCENE_RENDER_PASS scene=00_ebpf_event_journey frames=24 gif_bytes=507979
EBPF_VISUAL_SCENE_RENDER_PASS scene=01_ebpf_object_lifecycle frames=24 gif_bytes=520752
EBPF_VISUAL_SCENE_RENDER_PASS scene=02_ebpf_hook_journey frames=24 gif_bytes=596665
EBPF_VISUAL_RENDER_PASS scenes=3 frames=24 fps=8
```

### 4.3 渲染问题与修复

| 现象 | 定位 | 修复 |
|---|---|---|
| 01 桌面图出现局部黑块 | Chromium profile 深层路径与同进程跨场景合成状态共同触发 | 每帧使用短 `%TEMP%/ebpf-visual-*` profile；全量命令为每个场景启动独立 PowerShell worker |
| 单场景资产名为空 | PowerShell 变量名不区分大小写，参数 `$Scene` 与循环变量 `$scene` 冲突 | 循环变量改为 `$sceneItem`，场景定义改为 `PSCustomObject` |
| 移动端右侧被裁切 | headless Chrome 最小 CSS viewport 大于截图宽度，居中容器超出 390px 截图 | 520px 以下固定最大 378px 并左对齐，grid 子项设 `min-width: 0` |
| 播放时重复重建画布 | 每次 `draw()` 都重设 Canvas backing store | 仅像素尺寸变化时重建，Canvas 设置不透明背景 |

## 5. 桌面与移动端验证

桌面静态图：

- `docs/fundamentals/visuals/assets/00_ebpf_event_journey.png`
- `docs/fundamentals/visuals/assets/01_ebpf_object_lifecycle.png`
- `docs/fundamentals/visuals/assets/02_ebpf_hook_journey.png`

移动端证据：

- [00-mobile.png](results/00-mobile.png)
- [01-mobile.png](results/01-mobile.png)
- [02-mobile.png](results/02-mobile.png)

六张 PNG 均为 `rgb24`。FFmpeg `signalstats` 的 `YAVG` 范围为 `208.928` 到 `220.633`，三张桌面图的 `blackframe pblack=0`，不是全黑或全白画面；随后人工检查标题、节点、图例、控制条和知识面板。

## 6. 本地自动审计

```powershell
py -m py_compile tests/check_fundamentals.py tests/check_visual_assets.py
py tests/check_visual_assets.py
py tests/check_fundamentals.py
```

输出：

```text
EBPF_PYTHON_SYNTAX_PASS
EBPF_VISUAL_ASSET_AUDIT_PASS scenes=3 png=3 gif=3 canvas=3
EBPF_VISUAL_LEARNING_PILOT_COMPLETE
EBPF_OBSERVABILITY_DOC_AUDIT_PASS files=16 lines=1701 mermaid=62 links=pass
EBPF_OBSERVABILITY_FUNDAMENTALS_COMPLETE
```

## 7. 135 软件回归

Windows 归档不保留 Linux executable bit，解压后先恢复脚本权限：

```bash
cd /tmp/ebpf-visual-pilot-20260714-234624/track-ebpf-observability
chmod +x tests/*.sh project-linux-network-observability/scripts/*.sh
bash -n tests/*.sh project-linux-network-observability/scripts/02_run_observer.sh
python3 -m py_compile tests/check_fundamentals.py tests/check_visual_assets.py
SKIP_CLEAN=0 bash tests/software_regression.sh
```

关键 marker：

```text
EBPF_BUILD_PASS target=lab-libbpf-net-observer
EBPF_BUILD_PASS target=project-linux-network-observability
EBPF_OBSERVABILITY_FUNDAMENTALS_VISUALS_AND_BUILD_PASS
```

两个工程均执行 `make clean`，重新生成 `vmlinux.h`、编译 BPF object 和用户态程序。`project-linux-network-observability` 仍有一条既有 `strncpy` 可能截断告警，本次未改动该代码，不影响构建通过。

## 8. 135 Runtime smoke

```bash
printf '%s\n' '<sudo-password>' | sudo -S env EBPF_TEST_DURATION=3 \
  bash tests/runtime_regression.sh
```

关键 marker：

```text
EBPF_RUNTIME_CASE_PASS name=bpftrace_net_dev_queue
EBPF_RUNTIME_CASE_PASS name=libbpf_skb_observer
EBPF_RUNTIME_CASE_PASS name=project_net_observer
EBPF_OBSERVABILITY_RUNTIME_REGRESSION_PASS
```

`project_net_observer` 3 秒报告：

| 指标 | 结果 |
|---|---:|
| RX | 171 |
| GRO | 13 |
| TX-QUEUE | 163 |
| TX-XMIT | 163 |
| DROP | 0 |
| TX-QUEUE -> TX-XMIT | 100% |

## 9. 结论与边界

收口凭据扫描还发现两条历史脚本和三份历史文档保存了测试机明文密码。本次已统一改为可选 `SUDO_PASSWORD` 环境变量或交互式 `sudo`，文档完成脱敏，并在 135 上执行：

```text
EBPF_REMOTE_SHELL_SYNTAX_PASS
EBPF_CREDENTIAL_SCAN_PASS
```

结果：**PASS**。

- 00-02 已形成 Mermaid + PNG + GIF + Canvas 的混合学习层。
- 文档、资产、交互页面、桌面/移动端和 Linux build/runtime 均有可复现证据。
- runtime 只使用 loopback/现有 tracepoint，不修改 XDP、TC、网卡、PCI、hugepage 或 RDMA 配置。
- GIF 生成依赖本地 Chrome/Edge 与 FFmpeg；Linux 常规软件回归只审计已生成资产，不重复渲染。

最终 marker：`EBPF_VISUAL_LEARNING_PILOT_COMPLETE`。
