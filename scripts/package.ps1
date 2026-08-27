param(
  [ValidateSet("Debug", "Release")]
  [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$buildDir = Join-Path $projectRoot ("build-fubar-" + $Configuration.ToLowerInvariant() + "-mingw")
$exe = Join-Path $buildDir "FUBAR.exe"
$dist = Join-Path $projectRoot "dist"

& (Join-Path $PSScriptRoot "build.ps1") -Configuration $Configuration
if ($LASTEXITCODE -ne 0) { throw "Build failed" }
if (-not (Test-Path $exe)) { throw "Missing executable: $exe" }

New-Item -ItemType Directory -Force $dist | Out-Null
Copy-Item -LiteralPath $exe -Destination (Join-Path $dist "FUBAR.exe") -Force
Copy-Item -LiteralPath (Join-Path $projectRoot "README.md") -Destination $dist -Force
Copy-Item -LiteralPath (Join-Path $projectRoot "CHANGELOG.md") -Destination $dist -Force

$zip = Join-Path $projectRoot "FUBAR-Windows-x64.zip"
$packageFiles = @(
  (Join-Path $dist "FUBAR.exe"),
  (Join-Path $dist "README.md"),
  (Join-Path $dist "CHANGELOG.md")
)
Compress-Archive -Path $packageFiles -DestinationPath $zip -Force
Write-Host "Package: $zip"
