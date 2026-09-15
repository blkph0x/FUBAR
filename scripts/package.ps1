param(
  [ValidateSet("Debug", "Release")]
  [string]$Configuration = "Release",
  [string]$SdrTownControlDll = ""
)

$ErrorActionPreference = "Stop"
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$buildDir = Join-Path $projectRoot ("build-fubar-" + $Configuration.ToLowerInvariant() + "-mingw")
$exe = Join-Path $buildDir "FUBAR.exe"
$dist = Join-Path $projectRoot "dist"

& (Join-Path $PSScriptRoot "build.ps1") -Configuration $Configuration
if ($LASTEXITCODE -ne 0) { throw "Build failed" }
if (-not (Test-Path $exe)) { throw "Missing executable: $exe" }

$controlDllCandidates = @()
if (-not [string]::IsNullOrWhiteSpace($SdrTownControlDll)) {
  $controlDllCandidates += $SdrTownControlDll
}
if (-not [string]::IsNullOrWhiteSpace($env:SDRTOWN_CONTROL_DLL)) {
  $controlDllCandidates += $env:SDRTOWN_CONTROL_DLL
}
$controlDllCandidates += (Join-Path $buildDir "SdrTownControl.dll")
$controlDll = $controlDllCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1

if (Test-Path $dist) {
  Remove-Item -LiteralPath $dist -Recurse -Force
}
New-Item -ItemType Directory -Force $dist | Out-Null
Copy-Item -LiteralPath $exe -Destination (Join-Path $dist "FUBAR.exe") -Force
if ($controlDll) {
  Copy-Item -LiteralPath $controlDll -Destination (Join-Path $dist "SdrTownControl.dll") -Force
} else {
  Write-Warning "SdrTownControl.dll was not found. The FUBAR website will show SDR Town control unavailable until the DLL is placed beside FUBAR.exe."
}
Copy-Item -LiteralPath (Join-Path $projectRoot "README.md") -Destination $dist -Force
Copy-Item -LiteralPath (Join-Path $projectRoot "CHANGELOG.md") -Destination $dist -Force

$zip = Join-Path $projectRoot "FUBAR-Windows-x64.zip"
$packageFiles = @(
  (Join-Path $dist "FUBAR.exe"),
  (Join-Path $dist "SdrTownControl.dll"),
  (Join-Path $dist "README.md"),
  (Join-Path $dist "CHANGELOG.md")
) | Where-Object { Test-Path $_ }
Compress-Archive -Path $packageFiles -DestinationPath $zip -Force
Write-Host "Package: $zip"
