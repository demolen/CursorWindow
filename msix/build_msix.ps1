param(
  [string]$Version = "1.0.0.0",
  [string]$Publisher = "CN=89D75148-AE6F-4537-B23A-D2534BABD00B",
  [string]$OutDir = "dist",
  [switch]$SkipSign,
  [switch]$Force
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$proj = Split-Path -Parent $root
$staging = Join-Path $root 'staging'
New-Item -ItemType Directory -Force -Path $staging | Out-Null

# Copy binaries and assets
Copy-Item -Force (Join-Path $proj 'ChaseTheCursor.exe') $staging
Copy-Item -Force (Join-Path $root 'AppxManifest.xml') $staging
Copy-Item -Recurse -Force (Join-Path $root 'assets') $staging

# Update version and publisher in manifest (robust XML edit)
$manifestPath = Join-Path $staging 'AppxManifest.xml'
[xml]$xml = Get-Content -Raw $manifestPath
$xml.Package.Identity.Version = $Version
$xml.Package.Identity.Publisher = $Publisher
$xml.Save($manifestPath)

# Find MakeAppx and SignTool (optional)
$kits = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin'
$makeAppx = Get-ChildItem -Path $kits -Recurse -Filter makeappx.exe -ErrorAction SilentlyContinue | Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
$signtool = Get-ChildItem -Path $kits -Recurse -Filter signtool.exe -ErrorAction SilentlyContinue | Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName

New-Item -ItemType Directory -Force -Path (Join-Path $root $OutDir) | Out-Null
$outMsix = Join-Path (Join-Path $root $OutDir) 'ChaseTheCursor.msix'

if (Test-Path $outMsix) {
  if ($Force) { Remove-Item -Force $outMsix }
}

if (-not $makeAppx) {
  Write-Warning 'MakeAppx.exe not found. Install Windows 10/11 SDK to build the MSIX.'
  return
}

# Always overwrite existing output (/o)
& $makeAppx pack /d $staging /p $outMsix /o | Write-Host

# Optional signing for local sideload testing
if (-not $SkipSign -and $signtool) {
  try {
    $certName = "CN=Nanane.Dev"
    $cert = Get-ChildItem Cert:\CurrentUser\My | Where-Object { $_.Subject -eq $certName } | Select-Object -First 1
    if (-not $cert) {
      $cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject $certName -CertStoreLocation Cert:\CurrentUser\My
    }
    & $signtool sign /fd SHA256 /a /n "Nanane.Dev" $outMsix | Write-Host
  } catch {
    Write-Warning "Signing failed (ok for Store submission). $_"
  }
}

Write-Host "MSIX built at: $outMsix"
