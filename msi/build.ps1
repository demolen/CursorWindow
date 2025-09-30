param(
  [string]$Wxs = "OpenInCursorDisplay.wxs",
  [string]$OutDir = "dist",
  [string]$Arch = "x64"
)

$ErrorActionPreference = 'Stop'

# Find WiX v3 tools
$wixBinCandidates = @()

# Known locations
$known = @(
  Join-Path ${env:ProgramFiles(x86)} 'WiX Toolset v3.14\bin'),
  (Join-Path ${env:ProgramFiles(x86)} 'WiX Toolset v3.11\bin')
$wixBinCandidates += $known | Where-Object { $_ -and (Test-Path $_) }

# Discover any WiX v3.* installs under Program Files and Program Files (x86)
$roots = @(${env:ProgramFiles(x86)}, ${env:ProgramFiles}) | Where-Object { $_ -and (Test-Path $_) }
foreach ($root in $roots) {
  Get-ChildItem -Path $root -Directory -Filter 'WiX Toolset v3.*' -ErrorAction SilentlyContinue |
    ForEach-Object { Join-Path $_.FullName 'bin' } |
    Where-Object { Test-Path $_ } |
    ForEach-Object { $wixBinCandidates += $_ }
}

$wixBinCandidates = $wixBinCandidates | Select-Object -Unique

if (-not $wixBinCandidates) {
  Write-Error "WiX v3 tools not found. Please install with: winget install -e --id WiXToolset.WiXToolset (run as Administrator)"
}

$wixBin = $wixBinCandidates | Select-Object -First 1
$candle = Join-Path $wixBin 'candle.exe'
$light  = Join-Path $wixBin 'light.exe'

if (-not (Test-Path $candle)) { Write-Error "candle.exe not found in $wixBin" }
if (-not (Test-Path $light))  { Write-Error "light.exe not found in $wixBin" }

New-Item -ItemType Directory -Path $OutDir -Force | Out-Null

$wixobj = [System.IO.Path]::ChangeExtension($Wxs, '.wixobj')
& $candle -nologo -ext WixUIExtension -ext WixUtilExtension -arch $Arch $Wxs
& $light -nologo -ext WixUIExtension -ext WixUtilExtension $wixobj -o (Join-Path $OutDir "ChaseTheCursor-$Arch.msi")
Write-Host "MSI built at: " (Resolve-Path (Join-Path $OutDir "OpenInCursorDisplay-$Arch.msi"))
