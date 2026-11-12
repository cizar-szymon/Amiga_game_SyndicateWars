# Restart the Syndicate Wars WinUAE machine (kills a hung game) and wait for AmiAgent.
# Usage: powershell -ExecutionPolicy Bypass -File tools/amiga/winuae_restart.ps1
# JIT config by default (fast functional tests); $env:SW_UAE selects another one
$cfg = "C:\amiga-dev\winuae\SyndWars_A1200_OS32_net_jit.uae"
if ($env:SW_UAE) { $cfg = $env:SW_UAE }
$exe = "C:\Program Files\WinUAE\winuae64.exe"
Get-Process winuae64 -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 2
Start-Process $exe -ArgumentList "-f", $cfg, "-s", "use_gui=no"
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
for ($i = 0; $i -lt 30; $i++) {
    Start-Sleep -Seconds 3
    $r = & python "$repo\tools\amiga_remote.py" --config C:/amiga-dev/amiagent-winuae.cfg ping 2>$null
    if ($LASTEXITCODE -eq 0) { "WinUAE ready: $r"; exit 0 }
}
"WinUAE: AmiAgent did not answer"
exit 1
