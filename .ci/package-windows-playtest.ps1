[CmdletBinding()]
param(
    [string]$BuildDirectory = "C:\tmp\ja2-sandbox-build",
    [string]$OutputDirectory = "C:\tmp\efa-release",
    [string]$MingwBin = "C:\msys64\mingw64\bin"
)

$ErrorActionPreference = "Stop"
$repository = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$build = [IO.Path]::GetFullPath($BuildDirectory)
$output = [IO.Path]::GetFullPath($OutputDirectory)
$mingw = [IO.Path]::GetFullPath($MingwBin)
$version = (Get-Content -LiteralPath (Join-Path $repository "escape-from-arulco-version") -Raw).Trim()

if ($version -notmatch '^\d+\.\d+\.\d+\.\d+$') {
    throw "Invalid Escape from Arulco version: $version"
}

$packageName = "Escape-from-Arulco-Playtest-v$version"
$stage = [IO.Path]::GetFullPath((Join-Path $output $packageName))
$runtime = Join-Path $stage "runtime"
$archive = [IO.Path]::GetFullPath((Join-Path $output "$packageName.zip"))

function Assert-ChildPath {
    param([string]$Candidate, [string]$Parent)
    $parentPrefix = [IO.Path]::GetFullPath($Parent).TrimEnd('\') + '\'
    $candidatePath = [IO.Path]::GetFullPath($Candidate)
    if (-not $candidatePath.StartsWith($parentPrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to modify a path outside the output directory: $candidatePath"
    }
}

Assert-ChildPath -Candidate $stage -Parent $output
Assert-ChildPath -Candidate $archive -Parent $output

foreach ($required in @(
    (Join-Path $build "ja2.exe"),
    (Join-Path $build "ja2-launcher.exe"),
    (Join-Path $build "externalized"),
    (Join-Path $build "mods")
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Missing release input: $required"
    }
}

New-Item -ItemType Directory -Path $output -Force | Out-Null
if (Test-Path -LiteralPath $stage) {
    Remove-Item -LiteralPath $stage -Recurse -Force
}
if (Test-Path -LiteralPath $archive) {
    Remove-Item -LiteralPath $archive -Force
}
New-Item -ItemType Directory -Path $runtime -Force | Out-Null

& (Join-Path $PSScriptRoot "package-windows-runtime.ps1") `
    -BuildDirectory $build -Destination $runtime -MingwBin $mingw

Copy-Item -LiteralPath (Join-Path $build "externalized") -Destination $runtime -Recurse -Force
$modsDestination = Join-Path $runtime "mods"
New-Item -ItemType Directory -Path $modsDestination -Force | Out-Null
Get-ChildItem -LiteralPath (Join-Path $build "mods") -Force |
    Where-Object { $_.Name -ne "test-json-dialogs" } |
    Copy-Item -Destination $modsDestination -Recurse -Force

$documents = @(
    @{ Source = "README.md"; Destination = "README.md" },
    @{ Source = "FEATURE_WIRING.md"; Destination = "FEATURE_WIRING.md" },
    @{ Source = "MODIFICATIONS.md"; Destination = "MODIFICATIONS.md" },
    @{ Source = "PLAYTESTING.md"; Destination = "PLAYTESTING.md" },
    @{ Source = "docs\RELEASE_$version.md"; Destination = "RELEASE_$version.md" },
    @{ Source = "SFI Source Code license agreement.txt"; Destination = "SFI Source Code license agreement.txt" },
    @{ Source = "dependencies\lib-SDL2-2.0.20-mingw\README-SDL.txt"; Destination = "README-SDL.txt" }
)
foreach ($document in $documents) {
    $source = Join-Path $repository $document.Source
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Missing release document: $source"
    }
    Copy-Item -LiteralPath $source -Destination (Join-Path $stage $document.Destination) -Force
}

$startLauncher = @'
@echo off
setlocal
cd /d "%~dp0runtime"
start "Escape from Arulco Playtest" "%~dp0runtime\ja2-launcher.exe"
'@
$startDirect = @'
@echo off
setlocal
cd /d "%~dp0runtime"
start "Escape from Arulco Playtest" "%~dp0runtime\ja2.exe" -window
'@
$collectCrash = @'
@echo off
setlocal
set "REPORT_DIR=%~dp0reports"
if not exist "%REPORT_DIR%" mkdir "%REPORT_DIR%"
if exist "%TEMP%\ja2.log" copy /Y "%TEMP%\ja2.log" "%REPORT_DIR%\ja2-last-session.log" >nul
if exist "%APPDATA%\JA2\Feedback\*.txt" copy /Y "%APPDATA%\JA2\Feedback\*.txt" "%REPORT_DIR%\" >nul
if exist "%APPDATA%\JA2\AssetCatalog\os0-assets.tsv" copy /Y "%APPDATA%\JA2\AssetCatalog\os0-assets.tsv" "%REPORT_DIR%\" >nul
if exist "%APPDATA%\JA2\SavedGames\Error.sav" copy /Y "%APPDATA%\JA2\SavedGames\Error.sav" "%REPORT_DIR%\" >nul
echo.
echo Reports collected in:
echo %REPORT_DIR%
explorer "%REPORT_DIR%"
'@

Set-Content -LiteralPath (Join-Path $stage "START_PLAYTEST.cmd") -Value $startLauncher -Encoding Ascii
Set-Content -LiteralPath (Join-Path $stage "START_GAME_DIRECT.cmd") -Value $startDirect -Encoding Ascii
Set-Content -LiteralPath (Join-Path $stage "COLLECT_LAST_CRASH.cmd") -Value $collectCrash -Encoding Ascii

$commit = (& git -C $repository rev-parse HEAD).Trim()
$buildInfo = @(
    "Escape from Arulco $version",
    "Git commit: $commit",
    "Packaged UTC: $([DateTime]::UtcNow.ToString('o'))",
    "Original Jagged Alliance 2 data is not included."
)
Set-Content -LiteralPath (Join-Path $stage "BUILD_INFO.txt") -Value $buildInfo -Encoding Ascii

Compress-Archive -LiteralPath $stage -DestinationPath $archive -CompressionLevel Optimal
$hash = Get-FileHash -LiteralPath $archive -Algorithm SHA256
Write-Host "Windows playtest package: $archive"
Write-Host "SHA256: $($hash.Hash)"
