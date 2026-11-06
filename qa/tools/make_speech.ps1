# Makes qa/runs/fixtures/speech.wav with Windows text-to-speech (no download), for CC-01.
Add-Type -AssemblyName System.Speech
$s = New-Object System.Speech.Synthesis.SpeechSynthesizer
$f = Join-Path $PSScriptRoot "..\runs\fixtures\speech.wav"
$s.Rate = -1
$s.SetOutputToWaveFile($f)
$s.Speak("Hello everyone, and welcome back to the stream! Today, today we are playing a haunted house game. Please remember to follow, and remember to drink some water.")
$s.Dispose()
"wrote $f"
