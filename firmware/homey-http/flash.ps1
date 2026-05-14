param(
  [string]$Port = "",
  [switch]$Monitor
)

$ErrorActionPreference = "Stop"
$ProjectDir = Split-Path -Parent $MyInvocation.MyCommand.Path

$uploadArgs = @("-m", "platformio", "run", "-d", $ProjectDir, "-t", "upload")
if ($Port) {
  $uploadArgs += @("--upload-port", $Port)
}

Write-Host "Flasher MSpa Homey HTTP firmware fra $ProjectDir"
if ($Port) {
  Write-Host "Bruker port: $Port"
} else {
  Write-Host "Ingen port angitt. PlatformIO prover automatisk deteksjon."
}

& python @uploadArgs

if ($LASTEXITCODE -ne 0) {
  exit $LASTEXITCODE
}

if ($Monitor) {
  $monitorArgs = @("-m", "platformio", "device", "monitor", "-d", $ProjectDir)
  if ($Port) {
    $monitorArgs += @("--port", $Port)
  }
  & python @monitorArgs
}
