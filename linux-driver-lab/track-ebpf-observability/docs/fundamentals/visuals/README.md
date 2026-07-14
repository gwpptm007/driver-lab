# eBPF Fundamentals 视觉学习层

该目录为 `00-02` 试点提供三层视觉交付：Markdown 可直接查看的 PNG/GIF、可暂停单步的 Canvas、以及可重复生成资产的脚本。

## 资产矩阵

| 主题 | 静态图 | 动画 | 交互页面 |
|---|---|---|---|
| 00 事件旅程 | `assets/00_ebpf_event_journey.png` | `assets/00_ebpf_event_journey.gif` | `interactive/00_mental_model.html` |
| 01 对象生命周期 | `assets/01_ebpf_object_lifecycle.png` | `assets/01_ebpf_object_lifecycle.gif` | `interactive/01_kernel_lifecycle.html` |
| 02 Hook 选型 | `assets/02_ebpf_hook_journey.png` | `assets/02_ebpf_hook_journey.gif` | `interactive/02_hook_selection.html` |

## 阅读方式

1. 在 Markdown 中先看 Mermaid/PNG，建立静态内核位置；
2. 播放 GIF，观察 ownership、引用或 packet context 随时间推进；
3. 浏览器打开对应 HTML，使用播放、暂停、单步和复位；
4. 每一步阅读右侧的执行上下文、源码锚点、误判和验证命令；
5. 回到内核源码使用 `rg` 验证，不把动画当作 ABI 或唯一实现。

## 重新生成

Windows PowerShell：

```powershell
cd linux-driver-lab/track-ebpf-observability
powershell -ExecutionPolicy Bypass -File `
  docs/fundamentals/visuals/tools/render_visuals.ps1
```

依赖：

- Google Chrome（优先）或 Microsoft Edge；
- `ffmpeg` 在 `PATH`；
- PowerShell 5.1+。

脚本使用隔离的临时浏览器 profile，并对每个场景使用 `?capture=1&frame=N` 生成确定性画面，默认捕获 24 帧、8 FPS。保留中间帧用于排障：

```powershell
powershell -ExecutionPolicy Bypass -File `
  docs/fundamentals/visuals/tools/render_visuals.ps1 `
  -FrameCount 24 -Fps 8 -KeepFrames
```

只重渲染一个场景时传入资产名，例如：

```powershell
powershell -ExecutionPolicy Bypass -File `
  docs/fundamentals/visuals/tools/render_visuals.ps1 `
  -Scene 01_ebpf_object_lifecycle
```

## 为什么保留 Mermaid

GIF 与 Canvas 擅长表达时间和状态，但不适合代码审查、全文搜索与小改动维护。Mermaid 仍负责结构总图，PNG/GIF 负责无需工具即可查看，Canvas 负责深入推演。

## 内核知识约束

每个场景必须包含：

- 用户态/内核态边界；
- 真实对象或结构体名称；
- 源码目录与核心函数；
- 当前执行上下文和可睡眠边界；
- 常见失败或误判；
- 至少四条可执行验证命令。

视觉资产 marker：`EBPF_VISUAL_LEARNING_PILOT_COMPLETE`。
