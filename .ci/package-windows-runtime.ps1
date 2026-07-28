param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDirectory,
    [Parameter(Mandatory = $true)]
    [string]$Destination,
    [string]$MingwBin = "C:\msys64\mingw64\bin"
)

$ErrorActionPreference = "Stop"
$build = [IO.Path]::GetFullPath($BuildDirectory)
$target = [IO.Path]::GetFullPath($Destination)
$mingw = [IO.Path]::GetFullPath($MingwBin)

New-Item -ItemType Directory -Path $target -Force | Out-Null

$executables = @("ja2.exe", "ja2-launcher.exe")
$runtimeDlls = @(
    "SDL2.dll",
    "libgcc_s_seh-1.dll",
    "libstdc++-6.dll",
    "libwinpthread-1.dll",
    "libfltk-1.4.dll",
    "libfltk_images-1.4.dll",
    "libpng16-16.dll",
    "libjpeg-8.dll",
    "zlib1.dll"
)

foreach ($name in $executables) {
    $source = Join-Path $build $name
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Missing build output: $source"
    }
    Copy-Item -LiteralPath $source -Destination (Join-Path $target $name) -Force
}

foreach ($name in $runtimeDlls) {
    $source = if ($name -eq "SDL2.dll") {
        Join-Path $mingw $name
    } else {
        Join-Path $mingw $name
    }
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Missing MinGW runtime dependency: $source"
    }
    Copy-Item -LiteralPath $source -Destination (Join-Path $target $name) -Force
}

Write-Host "Portable Windows runtime assembled at $target"
