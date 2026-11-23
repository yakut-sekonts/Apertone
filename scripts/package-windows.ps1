[CmdletBinding()]
param(
    [string]$BuildDirectory = "build-windows",
    [Parameter(Mandatory)][string]$Destination,
    [Parameter(Mandatory)][string]$QtBin
)

$ErrorActionPreference = "Stop"
$source = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$build = (Resolve-Path -LiteralPath $BuildDirectory).Path
$qt = (Resolve-Path -LiteralPath $QtBin).Path
$output = [IO.Path]::GetFullPath($Destination)
if (Test-Path -LiteralPath $output) {
    if (!(Test-Path -LiteralPath $output -PathType Container) -or
        (Get-ChildItem -LiteralPath $output -Force | Select-Object -First 1)) {
        throw "Choose an empty destination directory."
    }
}

& cmake --install $build --config Release --prefix $output
if ($LASTEXITCODE -ne 0) {
    throw "CMake installation failed ($LASTEXITCODE)."
}
$desktop = Join-Path $output "Apertone.exe"
& (Join-Path $qt "windeployqt.exe") --release --qmldir (Join-Path $source "apps/desktop/qml") `
    --add-plugin-types imageformats --include-plugins qoffscreen --dir $output $desktop
if ($LASTEXITCODE -ne 0) {
    throw "Qt deployment failed ($LASTEXITCODE)."
}
foreach ($required in @("Apertone.exe", "ApertoneUpdate.exe", "apertone-cli.exe", "libluma_raw.dll",
                         "Qt6Network.dll", "Qt6PrintSupport.dll", "Qt6Widgets.dll", "tls/qschannelbackend.dll",
                         "platforms/qwindows.dll", "platforms/qoffscreen.dll", "imageformats/qtiff.dll",
                         "imageformats/qwebp.dll", "onnxruntime.dll",
                         "Models/u2netp.onnx", "Models/skyseg.onnx", "Models/pphumanseg.onnx",
                         "Models/yunet.onnx", "Models/depth-anything-v2.onnx", "Models/drunet-color.onnx")) {
    if (!(Test-Path -LiteralPath (Join-Path $output $required) -PathType Leaf)) {
        throw "Deployment is missing $required."
    }
}

$commit = git -C $source rev-parse HEAD
if ($LASTEXITCODE -ne 0) {
    throw "Cannot identify the source commit."
}
$tree = git -C $source rev-parse 'HEAD^{tree}'
if ($LASTEXITCODE -ne 0) {
    throw "Cannot identify the source tree."
}
$version = (Get-Item -LiteralPath $desktop).VersionInfo.ProductVersion
@("Repository: https://github.com/yakut-sekonts/Apertone", "Commit: $commit",
  "Tree: $tree", "Platform: Windows x64", "Version: $version") |
    Set-Content -LiteralPath (Join-Path $output "BUILD-ORIGIN.txt") -Encoding utf8
Get-ChildItem -LiteralPath $output -File -Recurse -Force |
    ForEach-Object { [IO.Path]::GetRelativePath($output, $_.FullName).Replace('\', '/') } |
    Sort-Object -CaseSensitive |
    Set-Content -LiteralPath (Join-Path $output "UPDATE-FILES.txt") -Encoding utf8
Get-Item -LiteralPath $desktop
