# Runs 50-output with a local RTMP receiver (ffmpeg) and probes every recording afterwards.
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$exe = Join-Path $root "native\dist\LOBS-qa\LOBS.exe"
$run = Join-Path $PSScriptRoot ("runs\" + (Get-Date -Format "yyyyMMdd-HHmmss") + "-output")
$out = Join-Path $run "50-output"
New-Item -ItemType Directory -Force $out | Out-Null
$recv = Start-Process ffmpeg -ArgumentList "-hide_banner -loglevel error -listen 1 -i rtmp://127.0.0.1:1935/live/qa -c copy -y `"$out\received.flv`"" -PassThru -WindowStyle Hidden
$watch = Start-Job -ArgumentList $out, $recv.Id { param($o, $id) while (-not (Test-Path "$o\kill-receiver")) { Start-Sleep -Milliseconds 200 }; Stop-Process -Id $id -Force -ErrorAction SilentlyContinue }
$p = Start-Process $exe -ArgumentList "--scenariotest", "`"$PSScriptRoot\scenarios\50-output.js`"", "`"$out`"" -PassThru
if (-not $p.WaitForExit(15 * 60000)) { $p.Kill(); "TIMEOUT" }
Stop-Job $watch -ErrorAction SilentlyContinue; Remove-Job $watch -Force -ErrorAction SilentlyContinue
if (-not $recv.HasExited) { Stop-Process -Id $recv.Id -Force }
$r = Get-Content "$out\results.json" -Raw -Encoding utf8 | ConvertFrom-Json
"exit=$($p.ExitCode) status=$($r.status)"
foreach ($st in $r.stories) {
    "  {0,-8} {1,-8} {2}" -f $st.status.ToUpper(), $st.id, $st.title
    foreach ($c in $st.checks | Where-Object { -not $_.ok }) { "             x {0}: {1}" -f $c.name, $c.detail }
    foreach ($n in $st.notes) { "             . $n" }
}
"--- ffprobe"
Get-ChildItem "$out\rec" -File -ErrorAction SilentlyContinue | ForEach-Object {
    $j = ffprobe -v error -show_entries "format=duration:stream=codec_type,codec_name,width,height,avg_frame_rate" -of json $_.FullName | ConvertFrom-Json
    $v = $j.streams | Where-Object codec_type -eq video | Select-Object -First 1
    $a = $j.streams | Where-Object codec_type -eq audio | Select-Object -First 1
    "{0,-36} {1,6:N2}s  {2} {3}x{4} {5}  audio:{6}  {7:N0} KB" -f $_.Name, [double]$j.format.duration, $v.codec_name, $v.width, $v.height, $v.avg_frame_rate, $a.codec_name, ($_.Length / 1KB)
}
if (Test-Path "$out\received.flv") {
    $j = ffprobe -v error -show_entries "format=duration:stream=codec_type,codec_name,width,height" -of json "$out\received.flv" | ConvertFrom-Json
    "received.flv: {0:N2}s, streams: {1}" -f [double]$j.format.duration, (($j.streams | ForEach-Object { "$($_.codec_type)/$($_.codec_name)" }) -join ", ")
} else { "received.flv: none" }
"Results: $run"
