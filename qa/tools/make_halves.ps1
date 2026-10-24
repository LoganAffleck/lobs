# Makes qa/runs/fixtures/halves.png (400x200, left half red, right half blue) for _probe-crop-drag.
Add-Type -AssemblyName System.Drawing
$b = New-Object System.Drawing.Bitmap 400, 200
$g = [System.Drawing.Graphics]::FromImage($b)
$g.FillRectangle([System.Drawing.Brushes]::Red, 0, 0, 200, 200)
$g.FillRectangle([System.Drawing.Brushes]::Blue, 200, 0, 200, 200)
$f = Join-Path $PSScriptRoot "..\runs\fixtures\halves.png"
$b.Save($f)
"wrote $f"
