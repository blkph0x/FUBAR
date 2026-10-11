param([string]$Package = 'package')
$ErrorActionPreference = 'Stop'
# DEC-0219: one immutable companion dependency for build and release jobs.
$version = '0.2.138'
$headers = @{ 'User-Agent' = 'FUBAR-paired-release' }
if ($env:GH_TOKEN) { $headers.Authorization = "Bearer $env:GH_TOKEN" }
$release = Invoke-RestMethod -Headers $headers "https://api.github.com/repos/blkph0x/SDR_Town/releases/tags/v$version"
$name = "SDR_Town-$version-win64.zip"
$asset = @($release.assets | Where-Object name -eq $name)
$checksum = @($release.assets | Where-Object name -eq "$name.sha256")
if ($asset.Count -ne 1 -or $checksum.Count -ne 1) { throw 'Pinned Town package/checksum unavailable' }
Invoke-WebRequest $asset[0].browser_download_url -OutFile town.zip -MaximumRetryCount 3
Invoke-WebRequest $checksum[0].browser_download_url -OutFile town.sha256 -MaximumRetryCount 3
$expected = (Get-Content town.sha256 -Raw).Trim().Split(' ')[0]
if ($expected -notmatch '^[a-fA-F0-9]{64}$' -or (Get-FileHash town.zip).Hash -ne $expected) { throw 'Town package hash mismatch' }
Expand-Archive town.zip -DestinationPath town -Force
$info = Get-Content town/build-info.json -Raw | ConvertFrom-Json
if ($info.projectVersion -ne $version -or $info.branch -ne "release/v$version" -or -not $info.sourceCommit) { throw 'Unexpected Town source provenance' }
if ((Get-FileHash town/SDR_Town.exe).Hash -ne $info.executableSha256) { throw 'Town executable provenance mismatch' }
$inventory = Get-Content town/package-inventory.json -Raw | ConvertFrom-Json
if (@($inventory.releaseBlockers).Count) { throw 'Town distribution is not qualified' }
New-Item -ItemType Directory -Force $Package | Out-Null
foreach ($name in @('SdrTownControl.dll','MSVCP140.dll','VCRUNTIME140.dll','VCRUNTIME140_1.dll')) {
  Copy-Item (Join-Path town $name) (Join-Path $Package $name)
}
& "$PSScriptRoot/test_control_dll.ps1" -Path (Join-Path $Package SdrTownControl.dll)
# Keep all supplied component notices alongside the dynamically loaded module.
Copy-Item town/licenses (Join-Path $Package 'town-licenses') -Recurse -Force
New-Item -ItemType Directory -Force (Join-Path $Package 'licenses') | Out-Null
Copy-Item third_party/nlohmann/LICENSE.MIT (Join-Path $Package 'licenses/nlohmann-json-MIT.txt')
Copy-Item third_party/shine/COPYING (Join-Path $Package 'licenses/shine-COPYING.txt')
git archive --format=zip --output="$Package/FUBAR-corresponding-source.zip" HEAD
if ($LASTEXITCODE -ne 0) { throw 'Cannot export exact companion source' }
Copy-Item docs/REBUILDING.md (Join-Path $Package 'REBUILDING.md')
[ordered]@{
  version = $version; sourceCommit = $info.sourceCommit; workflowRun = $info.workflowRun
  portableSha256 = $expected
  controlDllSha256 = (Get-FileHash (Join-Path $Package SdrTownControl.dll)).Hash.ToLowerInvariant()
} | ConvertTo-Json | Set-Content (Join-Path $Package 'town-pair.json') -Encoding utf8
