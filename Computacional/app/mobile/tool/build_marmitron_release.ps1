$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$outputDirectory = Join-Path $projectRoot "build\app\outputs\flutter-apk"
$sourceApk = Join-Path $outputDirectory "app-release.apk"
$marmitronApk = Join-Path $outputDirectory "MARMITRON_3000-release.apk"

Push-Location $projectRoot
try {
    flutter build apk --release

    if (-not (Test-Path -LiteralPath $sourceApk)) {
        throw "APK release nao encontrado em: $sourceApk"
    }

    Copy-Item -LiteralPath $sourceApk -Destination $marmitronApk -Force
    Write-Host "APK pronto: $marmitronApk"
} finally {
    Pop-Location
}
