param(
  [string]$RepositoryName = "FUBAR",
  [string]$Tag = "v1.1.15"
)

$ErrorActionPreference = "Stop"
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$asset = Join-Path $projectRoot "FUBAR-Windows-x64.zip"

if (-not (Get-Command gh -ErrorAction SilentlyContinue)) {
  throw "GitHub CLI (gh) is not installed"
}

gh auth status
if ($LASTEXITCODE -ne 0) {
  throw "GitHub CLI is not authenticated. Run: gh auth login"
}

& (Join-Path $PSScriptRoot "package.ps1") -Configuration Release
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $asset)) {
  throw "Release package build failed"
}

Push-Location $projectRoot
try {
  $remoteExists = git remote get-url origin 2>$null
  if (-not $remoteExists) {
    gh repo create $RepositoryName --public --source . --remote origin --push `
      --description "Native Windows VOX audio monitor and recorder with stereo, channel splitting, and VB-CABLE support."
    if ($LASTEXITCODE -ne 0) { throw "GitHub repository creation failed" }
  } else {
    git push -u origin main
    if ($LASTEXITCODE -ne 0) { throw "Source push failed" }
  }

  git push origin $Tag
  if ($LASTEXITCODE -ne 0) { throw "Tag push failed" }

  $prevError = $ErrorActionPreference
  $ErrorActionPreference = "Continue"
  gh release view $Tag *> $null
  $releaseExists = $LASTEXITCODE -eq 0
  $ErrorActionPreference = $prevError
  if ($releaseExists) {
    gh release upload $Tag $asset --clobber
  } else {
    gh release create $Tag $asset --title "FUBAR $Tag" --notes-file CHANGELOG.md
  }
  if ($LASTEXITCODE -ne 0) { throw "GitHub release publication failed" }

  gh repo view --web
} finally {
  Pop-Location
}
