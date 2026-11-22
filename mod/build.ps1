# Builds TiberianDawn.dll (Release | Win32) and installs it into the Remastered Mods folder.
# Usage:  powershell -ExecutionPolicy Bypass -File build.ps1          (build + install)
#         powershell -ExecutionPolicy Bypass -File build.ps1 -NoInstall
param([switch]$NoInstall, [switch]$Rebuild)
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$proj = Join-Path $here '..\SOURCECODE\TIBERIANDAWN\TiberianDawn.vcxproj'

# Find MSBuild from the installed Visual Studio
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found - is Visual Studio installed?" }
$msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (-not $msbuild) { throw "MSBuild not found." }

if (Get-Process -Name InstanceServerG -ErrorAction SilentlyContinue) {
    throw "The game is still running (InstanceServerG.exe) - close it first so the DLL can be replaced."
}

$target = if ($Rebuild) { 'Rebuild' } else { 'Build' }
Write-Host "Building TiberianDawn (Release|Win32, $target)..." -ForegroundColor Cyan
& $msbuild $proj /t:$target /p:Configuration=Release /p:Platform=Win32 /m /nologo /v:minimal
if ($LASTEXITCODE -ne 0) { throw "Build failed (exit code $LASTEXITCODE) - nothing installed." }
Write-Host "Build succeeded." -ForegroundColor Green

if (-not $NoInstall) { & (Join-Path $here 'install.ps1') }
