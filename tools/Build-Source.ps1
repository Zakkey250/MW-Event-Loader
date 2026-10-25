[CmdletBinding()]
param(
    [string]$D3DXRoot = (Join-Path $PSScriptRoot '..\external\d3dx'),
    [string]$MSBuildPath,
    [switch]$RunTests,
    [switch]$HudOnly
)
$ErrorActionPreference = 'Stop'
$sourceRoot = Split-Path $PSScriptRoot -Parent
if (-not $MSBuildPath) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Install Visual Studio with the C++ desktop workload and MSVC v143 x86/x64 build tools.' }
    $MSBuildPath = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
}
if (-not $MSBuildPath -or -not (Test-Path -LiteralPath $MSBuildPath)) { throw 'MSBuild not found. Supply -MSBuildPath.' }
function Build-Project([string]$relativeProject, [string[]]$extraProperties = @()) {
    & $MSBuildPath (Join-Path $sourceRoot $relativeProject) /nologo /m:2 /nr:false /v:minimal /p:Configuration=Release /p:Platform=Win32 @extraProperties
    if ($LASTEXITCODE) { throw "Build failed: $relativeProject" }
}
if (-not $HudOnly) {
    foreach ($required in @('NFSMWNativeFreeRoamRacers\include\LimitBreakerResourceApi.h', 'World2MW\src\NeoAI\Driver.h')) {
        if (-not (Test-Path -LiteralPath (Join-Path $sourceRoot $required))) { throw 'Full Event Loader compilation requires external NFRR dependencies and unpublished World2MW headers, intentionally absent from this repository. Use -HudOnly to build HUD Lifecycle Fix; see docs/Source-Build.md.' }
    }
    $D3DXRoot = (Resolve-Path -LiteralPath $D3DXRoot).Path
    foreach ($required in @('build\native\include\d3dx9.h', 'build\native\release\lib\x86\d3dx9.lib')) {
        if (-not (Test-Path -LiteralPath (Join-Path $D3DXRoot $required))) { throw "D3DX package is incomplete: $required. See docs/Source-Build.md." }
    }
    Build-Project 'NFSMWEventLoader\src\NFSMWEventLoader.vcxproj' @('/p:TorchRuntime=true', "/p:D3DXRoot=$D3DXRoot")
}
Build-Project 'NFSMWHudLifecycleFix\NFSMWHudLifecycleFix.vcxproj'
if ($RunTests) {
    Build-Project 'NFSMWHudLifecycleFix\NFSMWHudLifecycleFix.vcxproj' @('/p:NativeHarness=true')
    & (Join-Path $sourceRoot 'NFSMWHudLifecycleFix\bin\LifecycleTests.exe')
    if ($LASTEXITCODE) { throw 'HUD lifecycle tests failed.' }
    if (-not $HudOnly) {
        foreach ($testName in @('CombatHealthTests', 'RouteProgressTests', 'PackageSelectionTests', 'DifficultyFieldTests', 'CollectionLookupCacheTests')) {
            Build-Project "NFSMWEventLoader\src\$testName.vcxproj" @("/p:OutDir=$sourceRoot\NFSMWEventLoader\build\tests\")
            & (Join-Path $sourceRoot "NFSMWEventLoader\build\tests\$testName.exe")
            if ($LASTEXITCODE) { throw "$testName failed." }
        }
    }
}
if ($HudOnly) { Write-Host 'Built NFSMWHudLifecycleFix.asi.' }
else { Write-Host 'Built MWEventLoader.asi and NFSMWHudLifecycleFix.asi.' }
Write-Host 'No game installation was modified.'
