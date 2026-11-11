# Run a SyndWarsFX PC build for a while, screenshot ONLY its window, then close it.
# Usage: tools/amiga/run_pc.ps1 -Exe <path to exe> [-Seconds 40] [-Shot out.png] [-GameArgs "-q -W -m 0,1"]
param(
    [Parameter(Mandatory = $true)][string]$Exe,
    [int]$Seconds = 40,
    [string]$Shot = "",
    [string]$GameArgs = "-q -W -m 0,1",
    [string]$GameDir = "$PSScriptRoot\..\..\Syndicate Wars\syndwarsfx"
)
$GameDir = (Resolve-Path $GameDir).Path
$exeName = "syndwarsfx-run.exe"
Copy-Item $Exe "$GameDir\$exeName" -Force
$p = Start-Process -FilePath "$GameDir\$exeName" -ArgumentList $GameArgs -WorkingDirectory $GameDir -PassThru
Start-Sleep -Seconds $Seconds
if ($Shot -ne "" -and -not $p.HasExited) {
    Add-Type -AssemblyName System.Drawing
    Add-Type @"
using System; using System.Runtime.InteropServices;
public class W { [StructLayout(LayoutKind.Sequential)] public struct R { public int L, T, Ri, B; }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out R r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h); }
"@
    $p.Refresh()
    $h = $p.MainWindowHandle
    [W]::SetForegroundWindow($h) | Out-Null
    Start-Sleep -Milliseconds 300
    $r = New-Object W+R
    [W]::GetWindowRect($h, [ref]$r) | Out-Null
    $w = $r.Ri - $r.L; $hh = $r.B - $r.T
    $bmp = New-Object System.Drawing.Bitmap $w, $hh
    [System.Drawing.Graphics]::FromImage($bmp).CopyFromScreen($r.L, $r.T, 0, 0, $bmp.Size)
    $bmp.Save($Shot)
}
if (-not $p.HasExited) {
    $p.CloseMainWindow() | Out-Null
    if (-not $p.WaitForExit(10000)) { Stop-Process -Id $p.Id; "killed (no clean exit)" }
}
"exit code: $($p.ExitCode)"
