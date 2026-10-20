# PJ-02: kill LOBS at random moments while it autosaves the stress project, then check that the
# autosaved document is still valid JSON. Usage: qa\tools\crash_autosave.ps1 [-Iterations 30]
param([int]$Iterations = 30)
$qa = Split-Path $PSScriptRoot -Parent
$exe = Join-Path (Split-Path $qa -Parent) "native\dist\LOBS-qa\LOBS.exe"
$out = Join-Path $qa ("runs\" + (Get-Date -Format "yyyyMMdd-HHmmss") + "-crash-autosave")
$doc = Join-Path $env:TEMP "lobs-selftest.lobs.json"
$rng = New-Object Random 42
$rows = @()
for ($i = 1; $i -le $Iterations; $i++) {
    $o = Join-Path $out "iter$i"; New-Item -ItemType Directory -Force $o | Out-Null
    $p = Start-Process $exe -ArgumentList "--scenariotest", "`"$qa\scenarios\_autosave-churn.js`"", "`"$o`"" -PassThru
    $t0 = Get-Date
    while (-not (Test-Path "$o\armed") -and ((Get-Date) - $t0).TotalSeconds -lt 120) { Start-Sleep -Milliseconds 100 }
    if (-not (Test-Path "$o\armed")) { Stop-Process -Id $p.Id -Force; "{0,3}  no autosave within 120 s; skipped" -f $i; continue }
    Start-Sleep -Milliseconds ($rng.Next(100, 6000))
    Stop-Process -Id $p.Id -Force; $p.WaitForExit()
    $size = if (Test-Path $doc) { (Get-Item $doc).Length } else { -1 }
    $valid = $false; $why = ""
    if ($size -gt 0) {
        $r = & python -I -c "import json,sys; d=json.load(open(sys.argv[1],encoding='utf-8')); print('ok' if d.get('version')==1 and len(d.get('scenes',{}))==200 else 'badshape')" $doc 2>&1
        $valid = "$r" -eq "ok"; $why = "$r".Split("`n")[-1]
    } else { $why = if ($size -eq 0) { "EMPTY FILE" } else { "MISSING" } }
    if (-not $valid) { Copy-Item $doc "$o\broken.lobs.json" -ErrorAction SilentlyContinue }
    $rows += [pscustomobject]@{ Iter = $i; Bytes = $size; Valid = $valid; Detail = $why }
    "{0,3}  {1,9} bytes  {2}  {3}" -f $i, $size, ($(if ($valid) { "valid" } else { "BROKEN" })), $why
}
$bad = ($rows | Where-Object { -not $_.Valid }).Count
"`n$bad of $Iterations kills left a broken autosave.  Results: $out"
$rows | ConvertTo-Json | Set-Content "$out\summary.json" -Encoding utf8
