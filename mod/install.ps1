# Copies the built DLL and mod files into the Remastered Mods folder.
# Run from this folder after a Release build:  powershell -ExecutionPolicy Bypass -File install.ps1
$ErrorActionPreference = 'Stop'
$here   = Split-Path -Parent $MyInvocation.MyCommand.Path
$dll    = Join-Path $here '..\SOURCECODE\bin\Win32\TiberianDawn.dll'
$target = Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'CnCRemastered\Mods\Tiberian_Dawn\JamesMod'

if (-not (Test-Path $dll)) { throw "No DLL at $dll - build Release|Win32 first." }

New-Item -ItemType Directory -Force -Path (Join-Path $target 'Data') | Out-Null
Copy-Item (Join-Path $here 'JamesMod\ccmod.json') $target -Force
# Keep your tuned INI if one is already installed
if (-not (Test-Path (Join-Path $target 'JAMESMOD.INI'))) {
    Copy-Item (Join-Path $here 'JamesMod\JAMESMOD.INI') $target
}
Copy-Item $dll (Join-Path $target 'Data\TiberianDawn.dll') -Force
Write-Host "Installed to $target"
