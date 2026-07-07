param(
    [string]$TargetDir
)

# Windows installation script for Scene Tree View OBS Plugin
# Supports both Standard and Portable installations.

$ErrorActionPreference = "Stop"
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

Write-Host "====================================================" -ForegroundColor Cyan
Write-Host "  Scene Tree View - Windows Installer               " -ForegroundColor Cyan
Write-Host "====================================================" -ForegroundColor Cyan
Write-Host ""

# Check if OBS is running
$obsProcesses = Get-Process -Name "obs64" -ErrorAction SilentlyContinue
if ($obsProcesses) {
    Write-Host "WARNING: OBS Studio is currently running!" -ForegroundColor Yellow
    $response = Read-Host "Do you want to close OBS Studio now? (y/n)"
    if ($response -eq 'y' -or $response -eq 'Y') {
        Write-Host "Closing OBS Studio..." -ForegroundColor Yellow
        $obsProcesses | Stop-Process -Force
        Start-Sleep -Seconds 2
    } else {
        Write-Host "Installation cancelled. Please close OBS Studio and run the script again." -ForegroundColor Red
        exit 1
    }
}

# Find OBS installations helper
function Get-ObsInstallations {
    $candidates = [System.Collections.Generic.List[string]]::new()
    
    # 1. From running process location (even if closed now, check if we found it before)
    # Note: Processes are closed above, but we query registry and common paths
    
    # 2. Registry paths
    $regPaths = @(
        "HKLM:\SOFTWARE\OBS Studio",
        "HKLM:\SOFTWARE\WOW6432Node\OBS Studio",
        "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OBS Studio",
        "HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\OBS Studio",
        "HKCU:\SOFTWARE\OBS Studio",
        "HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OBS Studio"
    )
    foreach ($key in $regPaths) {
        if (Test-Path $key) {
            $val = Get-ItemProperty -Path $key -Name "InstallDir" -ErrorAction SilentlyContinue
            if ($val -and $val.InstallDir) { $candidates.Add($val.InstallDir) }
            
            $val = Get-ItemProperty -Path $key -Name "UninstallString" -ErrorAction SilentlyContinue
            if ($val -and $val.UninstallString) {
                $unDir = Split-Path $val.UninstallString -Parent
                $candidates.Add($unDir)
            }
        }
    }
    
    # 3. Common paths (both standard and portable locations)
    $commonPaths = @(
        "C:\Program Files\obs-studio",
        "C:\Program Files (x86)\obs-studio",
        "C:\obs-studio",
        "$env:USERPROFILE\Desktop\obs-studio",
        "$env:USERPROFILE\Downloads\obs-studio"
    )
    foreach ($p in $commonPaths) {
        $candidates.Add($p)
    }

    # Filter out duplicates and invalid paths (must contain bin\64bit\obs64.exe)
    $validPaths = @()
    $seen = @{}
    foreach ($c in $candidates) {
        if ([string]::IsNullOrWhiteSpace($c)) { continue }
        try {
            $resolved = [System.IO.Path]::GetFullPath($c)
            if (-not $seen.ContainsKey($resolved.ToLower())) {
                $seen[$resolved.ToLower()] = $true
                $exePath = Join-Path $resolved "bin\64bit\obs64.exe"
                if (Test-Path $exePath) {
                    $validPaths += $resolved
                }
            }
        } catch {}
    }
    return $validPaths
}

$selectedPath = $TargetDir

# Detect paths if not passed in parameter
if ([string]::IsNullOrWhiteSpace($selectedPath)) {
    $paths = Get-ObsInstallations
    if ($paths.Count -eq 0) {
        Write-Host "No OBS Studio installations were auto-detected." -ForegroundColor Yellow
        $selectedPath = Read-Host "Please enter the path to your OBS Studio installation folder (e.g. C:\Program Files\obs-studio)"
        if ([string]::IsNullOrWhiteSpace($selectedPath)) {
            Write-Host "Error: Installation folder is required." -ForegroundColor Red
            exit 1
        }
    } elseif ($paths.Count -eq 1) {
        $selectedPath = $paths[0]
        $type = if (Test-Path (Join-Path $selectedPath "portable_mode.txt")) { "Portable" } else { "Standard" }
        Write-Host "Detected OBS Studio installation: $selectedPath ($type)" -ForegroundColor Green
    } else {
        Write-Host "Multiple OBS Studio installations detected:" -ForegroundColor Cyan
        for ($i = 0; $i -lt $paths.Count; $i++) {
            $p = $paths[$i]
            $type = if (Test-Path (Join-Path $p "portable_mode.txt")) { "Portable" } else { "Standard" }
            Write-Host "  $($i + 1)) $p ($type)"
        }
        Write-Host "  $($paths.Count + 1)) Custom Path..."
        
        $choice = 0
        while ($choice -lt 1 -or $choice -gt ($paths.Count + 1)) {
            $input = Read-Host "Select an installation [1-$($paths.Count + 1)]"
            [void][int]::TryParse($input, [ref]$choice)
        }
        
        if ($choice -eq ($paths.Count + 1)) {
            $selectedPath = Read-Host "Please enter custom OBS Studio installation path"
        } else {
            $selectedPath = $paths[$choice - 1]
        }
    }
}

# Resolve selectedPath to full absolute path
if (-not [string]::IsNullOrWhiteSpace($selectedPath)) {
    $selectedPath = [System.IO.Path]::GetFullPath($selectedPath)
}

# Check write permissions & self-elevate if needed
$needsElevation = $false
try {
    $testFile = Join-Path $selectedPath "test_write_perm.tmp"
    New-Item -ItemType File -Path $testFile -ErrorAction Stop | Out-Null
    Remove-Item $testFile -ErrorAction SilentlyContinue
} catch {
    $needsElevation = $true
}

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

if ($needsElevation -and -not $isAdmin) {
    Write-Host "Administrator privileges are required to write to: $selectedPath" -ForegroundColor Yellow
    Write-Host "Requesting elevation..." -ForegroundColor Yellow
    
    $scriptUrl = "https://raw.githubusercontent.com/anthonymendez/scene-tree-view/main/scripts/install-windows.ps1"
    Start-Process powershell -ArgumentList "-NoProfile -ExecutionPolicy Bypass -Command `"& { [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; irm $scriptUrl | iex } -TargetDir '$selectedPath' `"" -Verb RunAs
    exit
}

# Fetch the latest release info from GitHub API
Write-Host "Fetching latest release information from GitHub..."
$Release = Invoke-RestMethod -Uri "https://api.github.com/repos/anthonymendez/scene-tree-view/releases/latest"
$Asset = $Release.assets | Where-Object { $_.name -like "*windows-x64.zip" } | Select-Object -First 1

if (-not $Asset) {
    Write-Host "Error: Could not find Windows zip asset in the latest release." -ForegroundColor Red
    exit 1
}

$DownloadUrl = $Asset.browser_download_url

# Create temporary folder
$tempDir = Join-Path $env:TEMP "stv-install"
if (Test-Path $tempDir) { Remove-Item $tempDir -Recurse -Force }
New-Item -ItemType Directory -Force -Path $tempDir | Out-Null

$zipPath = Join-Path $tempDir "plugin.zip"
Write-Host "Downloading latest release: $($Asset.name)..."
Invoke-WebRequest -Uri $DownloadUrl -OutFile $zipPath

Write-Host "Extracting files..."
Expand-Archive -Path $zipPath -DestinationPath (Join-Path $tempDir "extracted") -Force

# Setup target paths
$targetBinDir = Join-Path $selectedPath "obs-plugins\64bit"
$targetDataDir = Join-Path $selectedPath "data\obs-plugins\obs_scene_tree_view\locale"

# Ensure target directories exist
New-Item -ItemType Directory -Force -Path $targetBinDir | Out-Null
New-Item -ItemType Directory -Force -Path $targetDataDir | Out-Null

# Copy files from extracted archive
# Archive has folders like: obs-studio\obs-plugins\64bit\... and obs-studio\data\obs-plugins\obs_scene_tree_view\locale\...
$srcBinDir = Join-Path $tempDir "extracted\obs-studio\obs-plugins\64bit"
$srcDataDir = Join-Path $tempDir "extracted\obs-studio\data\obs-plugins\obs_scene_tree_view\locale"

if (-not (Test-Path $srcBinDir)) {
    # Fallback in case archive root structure changes slightly
    $srcBinDir = Get-ChildItem -Path $tempDir -Recurse -Filter "obs_scene_tree_view.dll" | Select-Object -First 1 | Split-Path -Parent
}

if ($srcBinDir -and (Test-Path $srcBinDir)) {
    Copy-Item -Path (Join-Path $srcBinDir "*") -Destination $targetBinDir -Force
} else {
    Write-Host "Error: Binary folder not found in archive." -ForegroundColor Red
    exit 1
}

if (-not (Test-Path $srcDataDir)) {
    # Fallback in case archive root structure changes slightly
    $srcDataDir = Get-ChildItem -Path $tempDir -Recurse -Filter "*.ini" | Select-Object -First 1 | Split-Path -Parent
}

if ($srcDataDir -and (Test-Path $srcDataDir)) {
    Copy-Item -Path (Join-Path $srcDataDir "*.ini") -Destination $targetDataDir -Force
} else {
    Write-Host "Warning: Locale folder not found in archive." -ForegroundColor Yellow
}

# Cleanup temp files
Remove-Item $tempDir -Recurse -Force

$installType = if (Test-Path (Join-Path $selectedPath "portable_mode.txt")) { "Portable" } else { "Standard" }
Write-Host ""
Write-Host "✔ Installation completed successfully to $selectedPath ($installType)!" -ForegroundColor Green
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Cyan
Write-Host "  1. Start/Restart OBS Studio."
Write-Host "  2. Go to View -> Docks -> Scene Tree View to check and enable the dock."
Write-Host "  3. If it doesn't show up, try View -> Docks -> Reset UI."
Write-Host ""
Write-Host "Press any key to exit..."
[void][Console]::ReadKey($true)
