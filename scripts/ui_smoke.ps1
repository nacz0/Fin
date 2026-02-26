param(
    [string]$ExePath = "build-fastener/Debug/Fin.exe",
    [string]$ActionsPath = "scripts/ui/smoke_actions.json",
    [string]$OutputDir = "artifacts/ui",
    [string]$WindowTitleContains = "Fin - Fast IDE",
    [int]$StartupTimeoutSec = 20,
    [int]$StepDelayMs = 300,
    [switch]$KeepOpen,
    [switch]$DryRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Resolve-AbsolutePath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [string]$BaseDir
    )

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }
    return (Join-Path $BaseDir $Path)
}

function Wait-ForMainWindow {
    param(
        [Parameter(Mandatory = $true)]
        [System.Diagnostics.Process]$Process,
        [Parameter(Mandatory = $true)]
        [int]$TimeoutSec,
        [string]$TitleContains = ""
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSec)
    while ((Get-Date) -lt $deadline) {
        if ($Process.HasExited) {
            throw "Process exited before UI became ready."
        }

        $Process.Refresh()
        if ($Process.MainWindowHandle -ne 0) {
            if ([string]::IsNullOrWhiteSpace($TitleContains) -or $Process.MainWindowTitle -like "*$TitleContains*") {
                return $Process.MainWindowHandle
            }
        }
        Start-Sleep -Milliseconds 200
    }

    throw "Main window was not found within timeout ($TimeoutSec s)."
}

if (-not ("FinUiHarness.NativeMethods" -as [type])) {
    Add-Type @"
using System;
using System.Runtime.InteropServices;

namespace FinUiHarness {
    public static class NativeMethods {
        [StructLayout(LayoutKind.Sequential)]
        public struct RECT {
            public int Left;
            public int Top;
            public int Right;
            public int Bottom;
        }

        [DllImport("user32.dll")]
        public static extern bool SetForegroundWindow(IntPtr hWnd);

        [DllImport("user32.dll")]
        public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

        [DllImport("user32.dll")]
        public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);

        [DllImport("user32.dll")]
        public static extern bool SetCursorPos(int X, int Y);

        [DllImport("user32.dll")]
        public static extern void mouse_event(uint dwFlags, uint dx, uint dy, uint dwData, UIntPtr dwExtraInfo);

        public const int SW_RESTORE = 9;
        public const uint MOUSEEVENTF_LEFTDOWN = 0x0002;
        public const uint MOUSEEVENTF_LEFTUP = 0x0004;
    }
}
"@
}

Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms

function Get-WindowRect {
    param(
        [Parameter(Mandatory = $true)]
        [System.IntPtr]$WindowHandle
    )

    $rect = New-Object FinUiHarness.NativeMethods+RECT
    if (-not [FinUiHarness.NativeMethods]::GetWindowRect($WindowHandle, [ref]$rect)) {
        throw "GetWindowRect failed."
    }
    return $rect
}

function Save-WindowScreenshot {
    param(
        [Parameter(Mandatory = $true)]
        [System.IntPtr]$WindowHandle,
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $rect = Get-WindowRect -WindowHandle $WindowHandle
    $width = [Math]::Max(1, $rect.Right - $rect.Left)
    $height = [Math]::Max(1, $rect.Bottom - $rect.Top)

    $bitmap = New-Object System.Drawing.Bitmap($width, $height)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.CopyFromScreen($rect.Left, $rect.Top, 0, 0, $bitmap.Size)
        $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

function Resolve-ClickPoint {
    param(
        [Parameter(Mandatory = $true)]
        [pscustomobject]$Action,
        [Parameter(Mandatory = $true)]
        [FinUiHarness.NativeMethods+RECT]$Rect
    )

    $coordMode = "relative"
    if ($null -ne $Action.coordMode) {
        $coordMode = [string]$Action.coordMode
    }

    if ($coordMode -eq "absolute") {
        return @{
            X = [int]$Action.x
            Y = [int]$Action.y
        }
    }

    $width = $Rect.Right - $Rect.Left
    $height = $Rect.Bottom - $Rect.Top
    if ($width -le 0 -or $height -le 0) {
        throw "Invalid window size while resolving relative click point."
    }

    $xValue = [double]$Action.x
    $yValue = [double]$Action.y

    $x = $Rect.Left + [int][Math]::Round($xValue * $width)
    $y = $Rect.Top + [int][Math]::Round($yValue * $height)
    return @{
        X = $x
        Y = $y
    }
}

function Invoke-LeftClick {
    param(
        [int]$X,
        [int]$Y
    )

    [FinUiHarness.NativeMethods]::SetCursorPos($X, $Y) | Out-Null
    Start-Sleep -Milliseconds 40
    [FinUiHarness.NativeMethods]::mouse_event([FinUiHarness.NativeMethods]::MOUSEEVENTF_LEFTDOWN, 0, 0, 0, [System.UIntPtr]::Zero)
    Start-Sleep -Milliseconds 40
    [FinUiHarness.NativeMethods]::mouse_event([FinUiHarness.NativeMethods]::MOUSEEVENTF_LEFTUP, 0, 0, 0, [System.UIntPtr]::Zero)
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$exeAbs = Resolve-AbsolutePath -Path $ExePath -BaseDir $repoRoot
$actionsAbs = Resolve-AbsolutePath -Path $ActionsPath -BaseDir $repoRoot
$outputAbs = Resolve-AbsolutePath -Path $OutputDir -BaseDir $repoRoot
$runId = Get-Date -Format "yyyyMMdd_HHmmss"
$runOutputDir = Join-Path $outputAbs $runId

if ($DryRun) {
    Write-Host "Dry run enabled. Resolved paths:"
    Write-Host "  ExePath:      $exeAbs"
    Write-Host "  ActionsPath:  $actionsAbs"
    Write-Host "  RunOutputDir: $runOutputDir"
    exit 0
}

if (-not (Test-Path $exeAbs)) {
    throw "Executable not found: $exeAbs"
}
if (-not (Test-Path $actionsAbs)) {
    throw "Actions file not found: $actionsAbs"
}

New-Item -ItemType Directory -Path $runOutputDir -Force | Out-Null
$actions = Get-Content -Raw $actionsAbs | ConvertFrom-Json
if ($null -eq $actions) {
    throw "Actions file is empty: $actionsAbs"
}

$process = $null
try {
    $workingDir = Split-Path -Parent $exeAbs
    $process = Start-Process -FilePath $exeAbs -WorkingDirectory $workingDir -PassThru
    $mainWindowHandle = Wait-ForMainWindow -Process $process -TimeoutSec $StartupTimeoutSec -TitleContains $WindowTitleContains

    [FinUiHarness.NativeMethods]::ShowWindow($mainWindowHandle, [FinUiHarness.NativeMethods]::SW_RESTORE) | Out-Null
    [FinUiHarness.NativeMethods]::SetForegroundWindow($mainWindowHandle) | Out-Null
    Start-Sleep -Milliseconds 250

    $stepIndex = 0
    foreach ($action in $actions) {
        $stepIndex++
        $type = [string]$action.type

        switch ($type) {
            "sleep" {
                $ms = [int]$action.ms
                if ($ms -lt 0) { throw "Invalid sleep duration at step $stepIndex." }
                Start-Sleep -Milliseconds $ms
                continue
            }
            "activate" {
                [FinUiHarness.NativeMethods]::ShowWindow($mainWindowHandle, [FinUiHarness.NativeMethods]::SW_RESTORE) | Out-Null
                [FinUiHarness.NativeMethods]::SetForegroundWindow($mainWindowHandle) | Out-Null
            }
            "click" {
                $rect = Get-WindowRect -WindowHandle $mainWindowHandle
                $point = Resolve-ClickPoint -Action $action -Rect $rect
                Invoke-LeftClick -X $point.X -Y $point.Y
            }
            "keys" {
                [System.Windows.Forms.SendKeys]::SendWait([string]$action.value)
            }
            "screenshot" {
                $name = [string]$action.name
                if ([string]::IsNullOrWhiteSpace($name)) {
                    $name = "step_$stepIndex"
                }
                $safeName = ($name -replace '[^a-zA-Z0-9_\-]', "_")
                $path = Join-Path $runOutputDir ("{0:D2}_{1}.png" -f $stepIndex, $safeName)
                Save-WindowScreenshot -WindowHandle $mainWindowHandle -Path $path
            }
            default {
                throw "Unknown action type '$type' at step $stepIndex."
            }
        }

        if ($StepDelayMs -gt 0) {
            Start-Sleep -Milliseconds $StepDelayMs
        }
    }

    $summary = @{
        run_id = $runId
        exe_path = $exeAbs
        actions_path = $actionsAbs
        output_dir = $runOutputDir
        steps_count = @($actions).Count
        timestamp = (Get-Date).ToString("o")
    } | ConvertTo-Json -Depth 3
    Set-Content -Path (Join-Path $runOutputDir "summary.json") -Value $summary -Encoding UTF8

    Write-Host "UI smoke run completed."
    Write-Host "Artifacts: $runOutputDir"
    exit 0
} catch {
    Write-Error $_
    exit 1
} finally {
    if ($null -ne $process -and -not $process.HasExited -and -not $KeepOpen.IsPresent) {
        try {
            Stop-Process -Id $process.Id -Force
        } catch {
            Write-Warning "Failed to stop process PID $($process.Id)."
        }
    }
}
