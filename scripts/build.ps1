param(
  [ValidateSet("Debug", "Release")]
  [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$buildDir = Join-Path $projectRoot ("build-" + $Configuration.ToLowerInvariant() + "-mingw")
$generator = "MinGW Makefiles"

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
  throw "cmake was not found in PATH"
}

$compilerCommand = Get-Command g++.exe -ErrorAction SilentlyContinue
$fallbackCompiler = Join-Path $env:USERPROFILE "gcc\bin\g++.exe"
$compiler = if ($compilerCommand) { $compilerCommand.Source } elseif (Test-Path $fallbackCompiler) { $fallbackCompiler } else { $null }
if (-not $compiler) { throw "g++.exe was not found in PATH or $fallbackCompiler" }

$gccBin = Split-Path $compiler -Parent
$env:PATH = "$gccBin;$env:PATH"
$mingwMake = Join-Path $gccBin "mingw32-make.exe"
$plainMake = Join-Path $gccBin "make.exe"
$makeProgram = if (Test-Path $mingwMake) { $mingwMake } elseif (Test-Path $plainMake) { $plainMake } else { $null }
if (-not $makeProgram) { throw "Neither mingw32-make.exe nor make.exe was found beside $compiler" }

$compiler = $compiler.Replace('\', '/')
$makeProgram = $makeProgram.Replace('\', '/')

cmake -S $projectRoot -B $buildDir -G $generator `
  "-DCMAKE_BUILD_TYPE=$Configuration" `
  "-DCMAKE_CXX_COMPILER=$compiler" `
  "-DCMAKE_MAKE_PROGRAM=$makeProgram"
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed with exit code $LASTEXITCODE" }

cmake --build $buildDir --parallel
if ($LASTEXITCODE -ne 0) { throw "Build failed with exit code $LASTEXITCODE" }

ctest --test-dir $buildDir --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "Tests failed with exit code $LASTEXITCODE" }

Write-Host "Built: $(Join-Path $buildDir 'AudioVox.exe')"
