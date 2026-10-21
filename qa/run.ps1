# Runs QA scenarios against the QA build and prints a summary.
#   qa\run.ps1                    all scenarios in qa\scenarios
#   qa\run.ps1 10-linking         one scenario (name without .js)
# Output: qa\runs\<timestamp>\<scenario>\results.json (+ snapshots, recordings, profile)
param([string[]]$Names, [int]$TimeoutMin = 40)

$root = Split-Path $PSScriptRoot -Parent
$exe = Join-Path $root "native\dist\LOBS-qa\LOBS.exe"
if (-not (Test-Path $exe)) { Write-Error "Build it first: qa\build-qa.bat"; exit 2 }
$scripts = if ($Names) { $Names | ForEach-Object { Join-Path $PSScriptRoot "scenarios\$_.js" } }
           else { Get-ChildItem (Join-Path $PSScriptRoot "scenarios") -Filter *.js | Where-Object { $_.Name -notlike "_*" } | ForEach-Object FullName }
$run = Join-Path $PSScriptRoot ("runs\" + (Get-Date -Format "yyyyMMdd-HHmmss"))
$summary = @()
foreach ($s in $scripts) {
    $name = [IO.Path]::GetFileNameWithoutExtension($s)
    $out = Join-Path $run $name
    New-Item -ItemType Directory -Force $out | Out-Null
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $p = Start-Process $exe -ArgumentList "--scenariotest", "`"$s`"", "`"$out`"" -PassThru
    $exited = $p.WaitForExit($TimeoutMin * 60000)
    if (-not $exited) { $p.Kill(); $code = "TIMEOUT" } else { $code = $p.ExitCode }
    $r = $null
    if (Test-Path "$out\results.json") { $r = Get-Content "$out\results.json" -Raw -Encoding utf8 | ConvertFrom-Json }
    # Exit codes 0/1 come from the runner; anything else (or status != done) means LOBS died mid-run.
    $crashed = ($code -ne 0 -and $code -ne 1) -or -not $r -or $r.status -ne "done"
    $summary += [pscustomobject]@{
        Scenario = $name; Exit = $code; Crashed = $crashed; Seconds = [int]$sw.Elapsed.TotalSeconds
        Pass = $r.totals.pass; Fail = $r.totals.fail; Other = $r.totals.other
    }
    if ($r) {
        foreach ($st in $r.stories) {
            $mark = switch ($st.status) { "pass" { "PASS" } "fail" { "FAIL" } "running" { "DIED" } default { $st.status.ToUpper() } }
            Write-Host ("  {0,-8} {1,-8} {2}" -f $mark, $st.id, $st.title)
            foreach ($c in $st.checks | Where-Object { -not $_.ok }) { Write-Host ("             x {0}: {1}" -f $c.name, $c.detail) }
        }
    }
}
$summary | Format-Table -AutoSize
Write-Host "Results: $run"
