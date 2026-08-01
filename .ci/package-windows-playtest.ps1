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

if ($version -notmatch '^\d+\.\d+\.\d+(?:\.\d+)?$') {
    throw "Invalid Escape from Arulco version: $version"
}

# A release-named archive is evidence for one exact source revision. Refuse to
# overwrite that identity with local edits or with a different tag; development
# builds use BUILD_AND_START_LOCAL.cmd instead.
$commit = (& git -C $repository rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or -not $commit) {
    throw "Cannot resolve the Git commit for release packaging."
}
$workingTreeChanges = @(& git -C $repository status --porcelain --untracked-files=normal)
if ($LASTEXITCODE -ne 0) {
    throw "Cannot inspect the Git working tree for release packaging."
}
if ($workingTreeChanges.Count -ne 0) {
    throw "Refusing to create a release-named package from a dirty working tree."
}
$expectedTag = "v$version"
$headTags = @(& git -C $repository tag --points-at HEAD)
if ($LASTEXITCODE -ne 0 -or $expectedTag -notin $headTags) {
    throw "Refusing to package commit $commit as ${expectedTag}: HEAD is not tagged $expectedTag."
}

$packageName = "Escape-from-Arulco-Playtest-v$version"
$stage = [IO.Path]::GetFullPath((Join-Path $output $packageName))
$runtime = Join-Path $stage "runtime"
$archive = [IO.Path]::GetFullPath((Join-Path $output "$packageName.zip"))
$hashFile = "$archive.sha256"

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
Assert-ChildPath -Candidate $hashFile -Parent $output

$cache = Join-Path $build "CMakeCache.txt"
if (-not (Test-Path -LiteralPath $cache -PathType Leaf)) {
    throw "Build directory is not configured: $cache"
}
$homeEntry = Get-Content -LiteralPath $cache |
    Where-Object { $_ -like 'CMAKE_HOME_DIRECTORY:INTERNAL=*' } |
    Select-Object -First 1
if (-not $homeEntry) {
    throw "Build cache does not identify its source repository: $cache"
}
$configuredSource = [IO.Path]::GetFullPath(($homeEntry -split '=', 2)[1].Replace('/', '\'))
if (-not [string]::Equals($configuredSource.TrimEnd('\'), $repository.TrimEnd('\'),
        [StringComparison]::OrdinalIgnoreCase)) {
    throw "Build directory belongs to '$configuredSource', not '$repository'."
}

# Rebuild from the clean, tagged source immediately before staging. Merely
# finding old executables in an arbitrary build directory is not evidence that
# the archive represents the commit written to BUILD_INFO.txt.
$cmake = Join-Path $mingw "cmake.exe"
if (-not (Test-Path -LiteralPath $cmake -PathType Leaf)) {
    throw "CMake was not found at $cmake"
}
$previousPath = $env:PATH
try {
    $env:PATH = "$mingw;$previousPath"
    # file(READ) inputs are not guaranteed to retrigger older generated build
    # trees. Configure explicitly so the binary always embeds this tagged
    # release's Escape from Arulco version before it is rebuilt.
    & $cmake -S $repository -B $build
    if ($LASTEXITCODE -ne 0) {
        throw "Release configure failed with exit code $LASTEXITCODE."
    }
    & $cmake --build $build --target ja2 ja2-launcher --parallel 2
    if ($LASTEXITCODE -ne 0) {
        throw "Release build failed with exit code $LASTEXITCODE."
    }
}
finally {
    $env:PATH = $previousPath
}

foreach ($required in @(
    (Join-Path $build "ja2.exe"),
    (Join-Path $build "ja2-launcher.exe"),
    (Join-Path $repository "assets\externalized"),
    (Join-Path $repository "assets\mods")
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
if (Test-Path -LiteralPath $hashFile) {
    Remove-Item -LiteralPath $hashFile -Force
}
New-Item -ItemType Directory -Path $runtime -Force | Out-Null

& (Join-Path $PSScriptRoot "package-windows-runtime.ps1") `
    -BuildDirectory $build -Destination $runtime -MingwBin $mingw

# Stage data from the clean, tagged source rather than trusting a potentially
# stale build-tree mirror. The runtime destination was created empty above, so
# removed assets cannot survive into the archive.
Copy-Item -LiteralPath (Join-Path $repository "assets\externalized") -Destination $runtime -Recurse -Force
$modsDestination = Join-Path $runtime "mods"
New-Item -ItemType Directory -Path $modsDestination -Force | Out-Null
Get-ChildItem -LiteralPath (Join-Path $repository "assets\mods") -Force |
    Where-Object { $_.Name -ne "test-json-dialogs" } |
    Copy-Item -Destination $modsDestination -Recurse -Force

$documents = @(
    @{ Source = "README.md"; Destination = "README.md" },
    @{ Source = "FEATURE_WIRING.md"; Destination = "FEATURE_WIRING.md" },
    @{ Source = "MODIFICATIONS.md"; Destination = "MODIFICATIONS.md" },
    @{ Source = "PLAYTESTING.md"; Destination = "PLAYTESTING.md" },
    @{ Source = "THIRD_PARTY_NOTICES.md"; Destination = "THIRD_PARTY_NOTICES.md" },
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

$buildInfo = @(
    "Escape from Arulco $version",
    "Git commit: $commit",
    "Packaged UTC: $([DateTime]::UtcNow.ToString('o'))",
    "Original Jagged Alliance 2 data is not included."
)
Set-Content -LiteralPath (Join-Path $stage "BUILD_INFO.txt") -Value $buildInfo -Encoding Ascii

Compress-Archive -LiteralPath $stage -DestinationPath $archive -CompressionLevel Optimal
$hash = Get-FileHash -LiteralPath $archive -Algorithm SHA256
Set-Content -LiteralPath $hashFile `
    -Value "$($hash.Hash.ToLowerInvariant())  $([IO.Path]::GetFileName($archive))" `
    -Encoding Ascii
Write-Host "Windows playtest package: $archive"
Write-Host "SHA256: $($hash.Hash)"
Write-Host "SHA256 sidecar: $hashFile"
