param([string]$Package = 'package', [string]$WorkDirectory = '.')
$ErrorActionPreference = 'Stop'
# DEC-0223: the release asset, not just its mutable tag, is pinned.
$version = '0.2.139'
$expectedSha256 = '77646cf86282cd00ee4af8634b90469bff607f51022a1b3ed58995e0109b8efd'
$expectedSource = 'a45b2006db79a17ceff1966caca5131806b793f6'
$repo = Split-Path $PSScriptRoot -Parent
$work = [IO.Path]::GetFullPath($WorkDirectory)
if (-not [IO.Path]::IsPathRooted($Package)) { $Package = Join-Path $work $Package }
$town = Join-Path $work 'town'
$zip = Join-Path $work 'town.zip'
$checksumPath = Join-Path $work 'town.sha256'
New-Item -ItemType Directory -Force $work | Out-Null
$headers = @{ 'User-Agent' = 'FUBAR-paired-release' }
if ($env:GH_TOKEN) { $headers.Authorization = "Bearer $env:GH_TOKEN" }
$release = Invoke-RestMethod -Headers $headers "https://api.github.com/repos/blkph0x/SDR_Town/releases/tags/v$version"
$name = "SDR_Town-$version-win64.zip"
$asset = @($release.assets | Where-Object name -eq $name)
$checksum = @($release.assets | Where-Object name -eq "$name.sha256")
if ($asset.Count -ne 1 -or $checksum.Count -ne 1) { throw 'Pinned Town package/checksum unavailable' }
Invoke-WebRequest $asset[0].browser_download_url -OutFile $zip -MaximumRetryCount 3
Invoke-WebRequest $checksum[0].browser_download_url -OutFile $checksumPath -MaximumRetryCount 3
$publishedHash = ((Get-Content $checksumPath -Raw).Trim() -split '\s+')[0]
if ($publishedHash -ne $expectedSha256 -or (Get-FileHash $zip).Hash -ne $expectedSha256) { throw 'Pinned Town package hash mismatch' }
Expand-Archive $zip -DestinationPath $town -Force
$info = Get-Content (Join-Path $town 'build-info.json') -Raw | ConvertFrom-Json
if ($info.projectVersion -ne $version -or $info.branch -ne "release/v$version" -or $info.sourceCommit -ne $expectedSource) { throw 'Unexpected Town source provenance' }
if ((Get-FileHash (Join-Path $town 'SDR_Town.exe')).Hash -ne $info.executableSha256) { throw 'Town executable provenance mismatch' }
$inventory = Get-Content (Join-Path $town 'package-inventory.json') -Raw | ConvertFrom-Json
if (@($inventory.releaseBlockers).Count) { throw 'Town distribution is not qualified' }
New-Item -ItemType Directory -Force $Package | Out-Null
foreach ($name in @('SdrTownControl.dll','MSVCP140.dll','VCRUNTIME140.dll','VCRUNTIME140_1.dll')) {
  $source = Join-Path $town $name
  if ((Get-FileHash $source).Hash -ne $inventory.files.$name.sha256) { throw "Town inventory mismatch: $name" }
  Copy-Item $source (Join-Path $Package $name)
}
& "$PSScriptRoot/test_control_dll.ps1" -Path (Join-Path $Package SdrTownControl.dll)
# Keep all supplied component notices alongside the dynamically loaded module.
Copy-Item (Join-Path $town 'licenses') (Join-Path $Package 'town-licenses') -Recurse -Force
New-Item -ItemType Directory -Force (Join-Path $Package 'licenses') | Out-Null
Copy-Item (Join-Path $repo 'third_party/nlohmann/LICENSE.MIT') (Join-Path $Package 'licenses/nlohmann-json-MIT.txt')
Copy-Item (Join-Path $repo 'third_party/shine/COPYING') (Join-Path $Package 'licenses/shine-COPYING.txt')
git -C $repo archive --format=zip --output="$Package/FUBAR-corresponding-source.zip" HEAD
if ($LASTEXITCODE -ne 0) { throw 'Cannot export exact companion source' }
Copy-Item (Join-Path $repo 'docs/REBUILDING.md') (Join-Path $Package 'REBUILDING.md')
[ordered]@{
  version = $version; sourceCommit = $info.sourceCommit; workflowRun = $info.workflowRun
  portableSha256 = $expectedSha256
  portableUrl = $asset[0].browser_download_url
  executableSha256 = $info.executableSha256
  controlDllSha256 = (Get-FileHash (Join-Path $Package SdrTownControl.dll)).Hash.ToLowerInvariant()
} | ConvertTo-Json | Set-Content (Join-Path $Package 'town-pair.json') -Encoding utf8
