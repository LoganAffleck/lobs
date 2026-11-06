# Benchmarks LOBS against Vanilla OBS (obs64.exe) under identical 1080p60 workloads:
#   1. Cold startup time (launch -> main window ready)
#   2. Idle preview (1080p60 scene with Display Capture + Color Filter + Text + Backdrop + Audio)
#   3. Active 1080p60 NVENC recording (or x264 fallback)
# Never touches the user's real %APPDATA%; uses an isolated temp APPDATA folder.

param(
    [int]$WarmupSec = 6,
    [int]$SampleSec = 15,
    [string]$OutJson = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$lobsExe = Join-Path $root "native\dist\LOBS\LOBS.exe"
$obsExe = "C:\Program Files\obs-studio\bin\64bit\obs64.exe"
$obsDir = "C:\Program Files\obs-studio\bin\64bit"

if (-not (Test-Path $lobsExe)) { throw "Missing $lobsExe" }
if (-not (Test-Path $obsExe)) { throw "Missing $obsExe" }

$benchRoot = Join-Path ([IO.Path]::GetTempPath()) "lobs-vs-obs-bench"
if (Test-Path $benchRoot) { Remove-Item -Recurse -Force $benchRoot -ErrorAction SilentlyContinue }
New-Item -ItemType Directory -Force $benchRoot | Out-Null
$recDir = Join-Path $benchRoot "recordings"
New-Item -ItemType Directory -Force $recDir | Out-Null

# Prepare isolated APPDATA for LOBS and portable directory structure for Vanilla OBS
$appData = Join-Path $benchRoot "AppData\Roaming"
$localAppData = Join-Path $benchRoot "AppData\Local"
New-Item -ItemType Directory -Force "$appData\LOBS\LOBS" | Out-Null
New-Item -ItemType Directory -Force "$localAppData\LOBS\LOBS" | Out-Null

$obsPortRoot = Join-Path $benchRoot "obs-portable"
$obsCfg = Join-Path $obsPortRoot "config\obs-studio"
New-Item -ItemType Directory -Force "$obsCfg\basic\profiles\Bench" | Out-Null
New-Item -ItemType Directory -Force "$obsCfg\basic\scenes" | Out-Null
New-Item -ItemType Directory -Force "$obsCfg\plugin_config" | Out-Null
foreach ($sub in @("bin", "core", "data", "obs-plugins")) {
    $srcDir = Join-Path "C:\Program Files\obs-studio" $sub
    if (Test-Path $srcDir) {
        New-Item -ItemType Junction -Path (Join-Path $obsPortRoot $sub) -Target $srcDir | Out-Null
    }
}
$obsExe = Join-Path $obsPortRoot "bin\64bit\obs64.exe"
$obsDir = Join-Path $obsPortRoot "bin\64bit"

$monPnp = Get-PnpDevice -Class Monitor -Status OK -ErrorAction SilentlyContinue | Select-Object -First 1
$monId = ""
if ($monPnp) {
    $parts = $monPnp.InstanceId -split '\\'
    $sub = ($parts[2] -split '&') | ForEach-Object { if ($_ -match '^UID') { $_ } else { $_.ToLower() } }
    $monId = "\\?\$($parts[0])#$($parts[1])#$($sub -join '&')#{e6f07b5f-ee97-4a90-b076-33f57bf4eaa7}"
}

# 1. Create LOBS benchmark document (current.lobs.json)
$lobsDoc = @{
    version = 1
    canvasW = 1920
    canvasH = 1080
    verticalOn = $false
    verticalW = 1080
    verticalH = 1920
    currentTransition = "tr_fade"
    transitions = @(
        @{ id = "tr_fade"; name = "Fade"; type = "fade"; durationMs = 300; settings = @{} },
        @{ id = "tr_cut"; name = "Cut"; type = "cut"; durationMs = 0; settings = @{} }
    )
    sceneOrder = @("scn_1", "scn_2")
    scenes = @{
        scn_1 = @{ id = "scn_1"; name = "Main Scene"; layers = @("inst_txt", "inst_disp", "inst_bg"); favorite = $false; mutedAudio = @() }
        scn_2 = @{ id = "scn_2"; name = "Secondary"; layers = @("inst_disp2", "inst_bg2"); favorite = $false; mutedAudio = @() }
    }
    sourceOrder = @("src_bg", "src_disp", "src_txt", "src_mic", "src_desk")
    sources = @{
        src_bg = @{ id = "src_bg"; name = "Background"; kind = "color"; hasAudio = $false; settings = @{ color = "#1e293b"; background = $true } }
        src_disp = @{ id = "src_disp"; name = "Display 1"; kind = "display"; hasAudio = $false; settings = @{ device = $monId } }
        src_txt = @{ id = "src_txt"; name = "Overlay Title"; kind = "text"; hasAudio = $false; settings = @{ text = "LIVE STREAM BENCHMARK"; font = "Segoe UI"; bold = $true; textColor = "#ffffff" } }
        src_mic = @{ id = "src_mic"; name = "Mic"; kind = "mic"; hasAudio = $true; settings = @{ device = "default" }; audio = @{ volume = 1.0; muted = $false; monitor = "off"; inRecording = $true; tracks = @($true,$true,$true,$true,$true,$true); filters = @() } }
        src_desk = @{ id = "src_desk"; name = "Desktop Audio"; kind = "desktop_audio"; hasAudio = $true; settings = @{ device = "default" }; audio = @{ volume = 1.0; muted = $false; monitor = "off"; inRecording = $true; tracks = @($true,$true,$true,$true,$true,$true); filters = @() } }
    }
    instances = @{
        inst_bg = @{ id = "inst_bg"; sceneId = "scn_1"; sourceId = "src_bg"; name = "Background"; kind = "source"; transform = @{ x = 0; y = 0; w = 1920; h = 1080; rot = 0; flipH = $false; flipV = $false }; crop = @{ l = 0; t = 0; r = 0; b = 0; radius = 0 }; visibility = @{ visible = $true; opacity = 1.0; locked = $true }; filters = @() }
        inst_disp = @{ id = "inst_disp"; sceneId = "scn_1"; sourceId = "src_disp"; name = "Display 1"; kind = "source"; transform = @{ x = 160; y = 90; w = 1600; h = 900; rot = 0; flipH = $false; flipV = $false }; crop = @{ l = 0; t = 0; r = 0; b = 0; radius = 0 }; visibility = @{ visible = $true; opacity = 1.0; locked = $false }; filters = @( @{ id = "flt_1"; type = "color"; enabled = $true; settings = @{ brightness = 0.0; contrast = 0.05; saturation = 0.05 } } ) }
        inst_txt = @{ id = "inst_txt"; sceneId = "scn_1"; sourceId = "src_txt"; name = "Overlay Title"; kind = "source"; transform = @{ x = 160; y = 40; w = 800; h = 72; rot = 0; flipH = $false; flipV = $false }; crop = @{ l = 0; t = 0; r = 0; b = 0; radius = 0 }; visibility = @{ visible = $true; opacity = 1.0; locked = $false }; filters = @() }
        inst_bg2 = @{ id = "inst_bg2"; sceneId = "scn_2"; sourceId = "src_bg"; name = "Background"; kind = "source"; transform = @{ x = 0; y = 0; w = 1920; h = 1080; rot = 0; flipH = $false; flipV = $false }; crop = @{ l = 0; t = 0; r = 0; b = 0; radius = 0 }; visibility = @{ visible = $true; opacity = 1.0; locked = $true }; filters = @() }
        inst_disp2 = @{ id = "inst_disp2"; sceneId = "scn_2"; sourceId = "src_disp"; name = "Display 1"; kind = "source"; transform = @{ x = 0; y = 0; w = 1920; h = 1080; rot = 0; flipH = $false; flipV = $false }; crop = @{ l = 0; t = 0; r = 0; b = 0; radius = 0 }; visibility = @{ visible = $true; opacity = 1.0; locked = $false }; filters = @() }
    }
    links = @{}
}
[IO.File]::WriteAllText("$appData\LOBS\LOBS\current.lobs.json", ($lobsDoc | ConvertTo-Json -Depth 12))

$lobsSettings = @{
    profile = "Default"
    video = @{ width = 1920; height = 1080; fps = 60 }
    stream = @{ service = "Twitch"; server = "auto"; key = ""; bitrateKbps = 6000; encoder = "jim_nvenc" }
    record = @{ folder = $recDir; format = "mkv"; quality = "stream"; bitrateKbps = 6000; alsoMp4 = $false }
}
[IO.File]::WriteAllText("$appData\LOBS\LOBS\settings.json", ($lobsSettings | ConvertTo-Json -Depth 6))

# 2. Create matching OBS profile & scene collection
$obsGlobalIni = @"
[General]
FirstRun=false
LastVersion=553648129
Pre19Defaults=false
Pre21Defaults=false
Pre23Defaults=false
Pre24.1Defaults=false
Pre31Migrated=true
Pre33.0Migrated=true
ConfirmOnExit=false
EnableAutoUpdates=false
WarnedAboutHideOBSFromCapture=true

[Basic]
Profile=Bench
ProfileDir=Bench
SceneCollection=Bench
SceneCollectionFile=Bench
ConfigOnNewProfile=false

[BasicWindow]
PreviewEnabled=true
AlwaysOnTop=false
SceneDuplicationMode=true
SwapScenesMode=true
WarnBeforeStartingStream=false
WarnBeforeStoppingStream=false
WarnBeforeStoppingRecord=false
"@
[IO.File]::WriteAllText("$obsCfg\global.ini", $obsGlobalIni)
[IO.File]::WriteAllText("$obsCfg\user.ini", $obsGlobalIni)

$obsBasicIni = @"
[General]
Name=Bench

[Video]
BaseCX=1920
BaseCY=1080
OutputCX=1920
OutputCY=1080
FPSType=0
FPSCommon=60
ScaleType=bicubic

[Audio]
SampleRate=48000
ChannelSetup=Stereo

[Output]
Mode=Simple

[SimpleOutput]
FilePath=$($recDir -replace '\\','/')
RecFormat2=mkv
VBitrate=6000
ABitrate=160
StreamEncoder=nvenc
RecQuality=Stream
"@
[IO.File]::WriteAllText("$obsCfg\basic\profiles\Bench\basic.ini", $obsBasicIni)

$obsSceneJson = @{
    current_scene = "Main Scene"
    current_program_scene = "Main Scene"
    scene_order = @( @{ name = "Main Scene" }, @{ name = "Secondary" } )
    name = "Bench"
    DesktopAudioDevice1 = @{
        id = "wasapi_output_capture"
        name = "Desktop Audio"
        settings = @{ device_id = "default" }
        volume = 1.0
        muted = $false
    }
    AuxAudioDevice1 = @{
        id = "wasapi_input_capture"
        name = "Mic/Aux"
        settings = @{ device_id = "default" }
        volume = 1.0
        muted = $false
    }
    sources = @(
        @{
            id = "color_source_v3"
            name = "Background"
            settings = @{ color = 4281936158; width = 1920; height = 1080 }
        },
        @{
            id = "monitor_capture"
            name = "Display 1"
            settings = @{ monitor_id = $monId }
            filters = @(
                @{
                    id = "color_filter_v2"
                    name = "Color Correction"
                    enabled = $true
                    settings = @{ brightness = 0.0; contrast = 0.05; saturation = 0.05 }
                }
            )
        },
        @{
            id = "text_gdiplus_v3"
            name = "Overlay Title"
            settings = @{
                text = "LIVE STREAM BENCHMARK"
                color = 16777215
                font = @{ face = "Segoe UI"; size = 72; style = "Bold"; flags = 1 }
            }
        },
        @{
            id = "scene"
            name = "Main Scene"
            settings = @{
                id_counter = 3
                items = @(
                    @{ name = "Background"; id = 1; visible = $true; pos = @{ x = 0.0; y = 0.0 }; scale = @{ x = 1.0; y = 1.0 }; bounds_type = 0 },
                    @{ name = "Display 1"; id = 2; visible = $true; pos = @{ x = 160.0; y = 90.0 }; bounds_type = 1; bounds = @{ x = 1600.0; y = 900.0 }; scale_filter = "bicubic" },
                    @{ name = "Overlay Title"; id = 3; visible = $true; pos = @{ x = 160.0; y = 40.0 }; scale = @{ x = 1.0; y = 1.0 }; scale_filter = "area" }
                )
            }
        },
        @{
            id = "scene"
            name = "Secondary"
            settings = @{
                id_counter = 2
                items = @(
                    @{ name = "Background"; id = 1; visible = $true; pos = @{ x = 0.0; y = 0.0 }; scale = @{ x = 1.0; y = 1.0 } },
                    @{ name = "Display 1"; id = 2; visible = $true; pos = @{ x = 0.0; y = 0.0 }; bounds_type = 1; bounds = @{ x = 1920.0; y = 1080.0 }; scale_filter = "bicubic" }
                )
            }
        }
    )
}
[IO.File]::WriteAllText("$obsCfg\basic\scenes\Bench.json", ($obsSceneJson | ConvertTo-Json -Depth 12))

function Measure-AppProcess {
    param(
        [string]$Label,
        [string]$Exe,
        [string]$WorkDir,
        [string[]]$ArgsList,
        [hashtable]$ExtraEnv = @{},
        [int]$Warmup = 6,
        [int]$Duration = 15
    )

    Write-Host ">>> Measuring $Label ..."
    Remove-Item -Force "$obsCfg\.sentinel\*" -ErrorAction SilentlyContinue
    $oldAppData = $env:APPDATA
    $oldLocalAppData = $env:LOCALAPPDATA
    $oldNoSetup = $env:LOBS_NO_SETUP
    $oldNativePrev = $env:LOBS_NATIVE_PREVIEW
    try {
        $env:APPDATA = $appData
        $env:LOCALAPPDATA = $localAppData
        $env:LOBS_NO_SETUP = "1"
        foreach ($k in $ExtraEnv.Keys) {
            [Environment]::SetEnvironmentVariable($k, $ExtraEnv[$k], "Process")
        }

        $swStart = [Diagnostics.Stopwatch]::StartNew()
        if ($ArgsList -and $ArgsList.Count -gt 0) {
            $p = Start-Process -FilePath $Exe -WorkingDirectory $WorkDir -ArgumentList $ArgsList -PassThru
        } else {
            $p = Start-Process -FilePath $Exe -WorkingDirectory $WorkDir -PassThru
        }
        $winReadyMs = 0
        while ($swStart.ElapsedMilliseconds -lt 15000) {
            $p.Refresh()
            if ($p.HasExited) { break }
            if ($p.MainWindowHandle -ne 0) {
                $winReadyMs = $swStart.ElapsedMilliseconds
                break
            }
            Start-Sleep -Milliseconds 25
        }

        Start-Sleep -Seconds $Warmup
        $p.Refresh()
        if ($p.HasExited) {
            throw "$Label exited prematurely with code $($p.ExitCode)"
        }

        $cores = [Environment]::ProcessorCount
        $cpu0 = $p.TotalProcessorTime.TotalSeconds
        # Also include child processes (e.g. obs-browser-page if any)
        $swSample = [Diagnostics.Stopwatch]::StartNew()
        $wsSamples = @()
        $privSamples = @()
        $gpu3dSamples = @()
        $gpuEncSamples = @()
        $vramSamples = @()

        while ($swSample.Elapsed.TotalSeconds -lt $Duration) {
            $p.Refresh()
            if ($p.HasExited) { break }
            $wsSamples += ($p.WorkingSet64 / 1MB)
            $privSamples += ($p.PrivateMemorySize64 / 1MB)

            try {
                $gpuCounters = Get-Counter "\GPU Engine(pid_$($p.Id)_*)\Utilization Percentage" -ErrorAction SilentlyContinue
                if ($gpuCounters) {
                    $g3d = ($gpuCounters.CounterSamples | Where-Object { $_.InstanceName -match "engtype_3D|engtype_Compute" } | Measure-Object -Property CookedValue -Sum).Sum
                    $gEnc = ($gpuCounters.CounterSamples | Where-Object { $_.InstanceName -match "engtype_VideoEncode" } | Measure-Object -Property CookedValue -Sum).Sum
                    $gpu3dSamples += [double]$g3d
                    $gpuEncSamples += [double]$gEnc
                }
                $memCounters = Get-Counter "\GPU Process Memory(pid_$($p.Id)_*)\Dedicated Usage" -ErrorAction SilentlyContinue
                if ($memCounters) {
                    $vram = ($memCounters.CounterSamples | Measure-Object -Property CookedValue -Sum).Sum / 1MB
                    $vramSamples += [double]$vram
                }
            } catch {}

            Start-Sleep -Milliseconds 1000
        }

        $p.Refresh()
        $wallSec = $swSample.Elapsed.TotalSeconds
        $cpuDelta = if (-not $p.HasExited) { $p.TotalProcessorTime.TotalSeconds - $cpu0 } else { 0 }
        $cpuSystemPct = [Math]::Round(($cpuDelta / $wallSec / $cores) * 100, 2)
        $cpuSingleCorePct = [Math]::Round(($cpuDelta / $wallSec) * 100, 2)

        # Close gracefully so OBS/LOBS writes final log summary
        if (-not $p.HasExited) {
            $null = $p.CloseMainWindow()
            if (-not $p.WaitForExit(5000)) {
                $p.Kill()
                $null = $p.WaitForExit(2000)
            }
        }

        $avgWs = [Math]::Round(($wsSamples | Measure-Object -Average).Average, 1)
        $avgPriv = [Math]::Round(($privSamples | Measure-Object -Average).Average, 1)
        $avgGpu3d = if ($gpu3dSamples.Count -gt 0) { [Math]::Round(($gpu3dSamples | Measure-Object -Average).Average, 2) } else { 0 }
        $avgGpuEnc = if ($gpuEncSamples.Count -gt 0) { [Math]::Round(($gpuEncSamples | Measure-Object -Average).Average, 2) } else { 0 }
        $avgVram = if ($vramSamples.Count -gt 0) { [Math]::Round(($vramSamples | Measure-Object -Average).Average, 1) } else { 0 }

        return [pscustomobject]@{
            Label          = $Label
            WindowReadyMs  = $winReadyMs
            CpuSysPct      = $cpuSystemPct
            Cpu1CorePct    = $cpuSingleCorePct
            Gpu3DPct       = $avgGpu3d
            GpuEncPct      = $avgGpuEnc
            WorkingSetMB   = $avgWs
            PrivateMB      = $avgPriv
            DedicatedVramMB= $avgVram
        }
    } finally {
        $env:APPDATA = $oldAppData
        $env:LOCALAPPDATA = $oldLocalAppData
        [Environment]::SetEnvironmentVariable("LOBS_NO_SETUP", $oldNoSetup, "Process")
        [Environment]::SetEnvironmentVariable("LOBS_NATIVE_PREVIEW", $oldNativePrev, "Process")
    }
}

$results = @()

# Idle Preview comparison
$results += Measure-AppProcess -Label "Vanilla OBS 33 (Idle Preview)" -Exe $obsExe -WorkDir $obsDir `
    -ArgsList @("--portable", "--collection", "Bench", "--profile", "Bench", "--disable-updater", "--disable-missing-files-check") `
    -Warmup $WarmupSec -Duration $SampleSec

$results += Measure-AppProcess -Label "LOBS (Idle Preview - SharedTex)" -Exe $lobsExe -WorkDir (Split-Path $lobsExe -Parent) `
    -ArgsList @() -Warmup $WarmupSec -Duration $SampleSec

$results += Measure-AppProcess -Label "LOBS (Idle Preview - NativeWin)" -Exe $lobsExe -WorkDir (Split-Path $lobsExe -Parent) `
    -ArgsList @() -ExtraEnv @{ LOBS_NATIVE_PREVIEW = "1" } -Warmup $WarmupSec -Duration $SampleSec

# Active 1080p60 Recording comparison
$results += Measure-AppProcess -Label "Vanilla OBS 33 (Recording 1080p60)" -Exe $obsExe -WorkDir $obsDir `
    -ArgsList @("--portable", "--collection", "Bench", "--profile", "Bench", "--startrecording", "--disable-updater", "--disable-missing-files-check") `
    -Warmup $WarmupSec -Duration 12

$results += Measure-AppProcess -Label "LOBS (Stream+Record 1080p60)" -Exe $lobsExe -WorkDir (Split-Path $lobsExe -Parent) `
    -ArgsList @("--streamtest", "rtmp://127.0.0.1:19359/live") -Warmup 4 -Duration 10

$results | Format-Table -AutoSize
if ($OutJson) {
    $results | ConvertTo-Json -Depth 5 | Set-Content -Path $OutJson -Encoding utf8
}
