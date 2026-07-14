param(
    [ValidateRange(8, 120)]
    [int]$FrameCount = 24,

    [ValidateRange(2, 30)]
    [int]$Fps = 8,

    [string[]]$Scene = @(),

    [switch]$Worker,

    [switch]$KeepFrames
)

$ErrorActionPreference = 'Stop'

# 全量渲染时让每个场景进入独立 PowerShell 进程，彻底隔离 Chromium 合成状态。
if ($Scene.Count -eq 0 -and -not $Worker) {
    $allScenes = @(
        '00_ebpf_event_journey',
        '01_ebpf_object_lifecycle',
        '02_ebpf_hook_journey'
    )
    foreach ($assetName in $allScenes) {
        $childArgs = @(
            '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $PSCommandPath,
            '-FrameCount', $FrameCount, '-Fps', $Fps, '-Scene', $assetName, '-Worker'
        )
        if ($KeepFrames) {
            $childArgs += '-KeepFrames'
        }
        & powershell @childArgs
        if ($LASTEXITCODE -ne 0) {
            throw "Visual worker failed: $assetName (exit=$LASTEXITCODE)"
        }
    }
    Write-Output "EBPF_VISUAL_RENDER_PASS scenes=3 frames=$FrameCount fps=$Fps"
    exit 0
}

$visualRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$interactiveDir = Join-Path $visualRoot 'interactive'
$assetDir = Join-Path $visualRoot 'assets'
$renderRoot = Join-Path $visualRoot '.render'

function Find-Browser {
    $candidates = @(
        'C:\Program Files\Google\Chrome\Application\chrome.exe',
        'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe',
        'C:\Program Files\Microsoft\Edge\Application\msedge.exe'
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }
    throw 'Chrome/Edge executable not found; cannot capture Canvas frames.'
}

function Find-Ffmpeg {
    $command = Get-Command ffmpeg -ErrorAction SilentlyContinue
    if ($null -eq $command) {
        throw 'ffmpeg not found in PATH; cannot build GIF assets.'
    }
    return $command.Source
}

function Invoke-Checked {
    param(
        [Parameter(Mandatory)] [string]$Executable,
        [Parameter(Mandatory)] [string[]]$Arguments,
        [Parameter(Mandatory)] [string]$Description
    )
    # Edge 属于 GUI 子系统程序，直接用 & 调用不会等待，也不会可靠设置 LASTEXITCODE。
    $startArgs = @{
        FilePath = $Executable
        ArgumentList = $Arguments
        WindowStyle = 'Hidden'
        Wait = $true
        PassThru = $true
    }
    $process = Start-Process @startArgs
    if ($process.ExitCode -ne 0) {
        throw "$Description failed (exit=$($process.ExitCode))"
    }
}

function Clear-RenderDirectory {
    param([Parameter(Mandatory)] [string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) {
        return
    }
    $resolved = (Resolve-Path -LiteralPath $Path).Path
    if (-not $resolved.StartsWith($renderRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to clean path outside render root: $resolved"
    }
    Get-ChildItem -LiteralPath $resolved -File | ForEach-Object {
        Remove-Item -LiteralPath $_.FullName -Force
    }
}

$browser = Find-Browser
$ffmpeg = Find-Ffmpeg
New-Item -ItemType Directory -Force -Path $assetDir, $renderRoot | Out-Null

$scenes = @(
    [pscustomobject]@{ Html = '00_mental_model.html'; Asset = '00_ebpf_event_journey' },
    [pscustomobject]@{ Html = '01_kernel_lifecycle.html'; Asset = '01_ebpf_object_lifecycle' },
    [pscustomobject]@{ Html = '02_hook_selection.html'; Asset = '02_ebpf_hook_journey' }
)

if ($Scene.Count -gt 0) {
    $scenes = @($scenes | Where-Object { $Scene -contains $_.Asset })
    if ($scenes.Count -eq 0) {
        throw 'No scene matched. Valid names: 00_ebpf_event_journey, 01_ebpf_object_lifecycle, 02_ebpf_hook_journey'
    }
}

foreach ($sceneItem in $scenes) {
    $htmlPath = (Resolve-Path (Join-Path $interactiveDir $sceneItem.Html)).Path
    $frameDir = Join-Path $renderRoot $sceneItem.Asset
    New-Item -ItemType Directory -Force -Path $frameDir | Out-Null
    Clear-RenderDirectory -Path $frameDir

    $fileUrl = ([System.Uri]$htmlPath).AbsoluteUri
    for ($frame = 0; $frame -lt $FrameCount; $frame++) {
        $framePath = Join-Path $frameDir ('frame-{0:D3}.png' -f $frame)
        $url = "${fileUrl}?capture=1&frame=${frame}"
        # 使用短临时路径，避免 Chromium profile 在 Windows 深层目录下初始化不完整。
        $profileName = "ebpf-visual-{0}-{1:D3}-{2}" -f $sceneItem.Asset, $frame, $PID
        $browserProfile = Join-Path $env:TEMP $profileName
        New-Item -ItemType Directory -Force -Path $browserProfile | Out-Null
        $browserArgs = @(
            '--headless=new',
            '--disable-gpu',
            '--disable-extensions',
            '--hide-scrollbars',
            '--no-first-run',
            '--no-default-browser-check',
            '--force-color-profile=srgb',
            "--user-data-dir=$browserProfile",
            '--run-all-compositor-stages-before-draw',
            '--virtual-time-budget=1200',
            '--window-size=1200,720',
            "--screenshot=$framePath",
            $url
        )
        Invoke-Checked -Executable $browser -Arguments $browserArgs -Description "capture $($sceneItem.Html) frame $frame"
        if (-not (Test-Path -LiteralPath $framePath)) {
            throw "Edge reported success but frame is missing: $framePath"
        }

        $resolvedProfile = (Resolve-Path -LiteralPath $browserProfile).Path
        $resolvedTemp = (Resolve-Path -LiteralPath $env:TEMP).Path
        if ((Split-Path -Parent $resolvedProfile) -ne $resolvedTemp `
            -or -not (Split-Path -Leaf $resolvedProfile).StartsWith('ebpf-visual-')) {
            throw "Refusing to clean unexpected browser profile: $resolvedProfile"
        }
        Remove-Item -LiteralPath $resolvedProfile -Recurse -Force
    }

    $staticPath = Join-Path $assetDir "$($sceneItem.Asset).png"
    Copy-Item -LiteralPath (Join-Path $frameDir 'frame-000.png') -Destination $staticPath -Force

    $inputPattern = Join-Path $frameDir 'frame-%03d.png'
    $palettePath = Join-Path $frameDir 'palette.png'
    $gifPath = Join-Path $assetDir "$($sceneItem.Asset).gif"

    Invoke-Checked -Executable $ffmpeg -Description "generate palette for $($sceneItem.Asset)" -Arguments @(
        '-y', '-loglevel', 'error', '-framerate', "$Fps", '-i', $inputPattern,
        '-vf', 'palettegen=stats_mode=diff', $palettePath
    )
    Invoke-Checked -Executable $ffmpeg -Description "generate GIF for $($sceneItem.Asset)" -Arguments @(
        '-y', '-loglevel', 'error', '-framerate', "$Fps", '-i', $inputPattern,
        '-i', $palettePath, '-lavfi', 'paletteuse=dither=bayer:bayer_scale=3',
        '-loop', '0', $gifPath
    )

    $gifBytes = (Get-Item -LiteralPath $gifPath).Length
    Write-Output "EBPF_VISUAL_SCENE_RENDER_PASS scene=$($sceneItem.Asset) frames=$FrameCount gif_bytes=$gifBytes"

    if (-not $KeepFrames) {
        Clear-RenderDirectory -Path $frameDir
        Remove-Item -LiteralPath $frameDir

    }
}

if (-not $KeepFrames -and (Test-Path -LiteralPath $renderRoot) `
    -and (Get-ChildItem -LiteralPath $renderRoot | Measure-Object).Count -eq 0) {
    Remove-Item -LiteralPath $renderRoot
}

if (-not $Worker) {
    Write-Output "EBPF_VISUAL_RENDER_PASS scenes=$($scenes.Count) frames=$FrameCount fps=$Fps"
}
