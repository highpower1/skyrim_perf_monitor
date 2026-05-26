# build_perf.ps1
$ErrorActionPreference = "Stop"

# 1. Verify VCPKG Environment
$VCPKG_PATH = $env:VCPKG_ROOT
if (-not $VCPKG_PATH) {
    Write-Host "WARNING: Environment variable 'VCPKG_ROOT' is not set." -ForegroundColor Yellow
    Write-Host "Attempting to auto-detect vcpkg in common locations..." -ForegroundColor Cyan
    $CommonPaths = @("C:\src\vcpkg", "C:\vcpkg", "$env:USERPROFILE\vcpkg")
    foreach ($Path in $CommonPaths) {
        if (Test-Path "$Path\vcpkg.exe") {
            $VCPKG_PATH = $Path
            $env:VCPKG_ROOT = $Path
            Write-Host "Found vcpkg at: $Path" -ForegroundColor Green
            break
        }
    }
    if (-not $VCPKG_PATH) {
        Write-Error "Could not find vcpkg. Please install it and set the VCPKG_ROOT environment variable."
    }
}

Write-Host "========== Phase 1: Generating CMake Project with vcpkg toolchain ==========" -ForegroundColor Green
if (Test-Path "$PSScriptRoot/build") {
    Write-Host "Removing existing build directory to clear cache..." -ForegroundColor Cyan
    Remove-Item -Recurse -Force "$PSScriptRoot/build"
}

cmake -B build -S . `
    "-DCMAKE_TOOLCHAIN_FILE=$VCPKG_PATH/scripts/buildsystems/vcpkg.cmake" `
    "-DVCPKG_TARGET_TRIPLET=x64-windows-static-md" `
    "-DCompiledPluginsPath=$PSScriptRoot/dist" `
    "-DCOPY_OUTPUT=ON" `
    "-DCMAKE_BUILD_TYPE=Release"


Write-Host "========== Phase 2: Compiling Skyrim Performance Monitor DLL via MSVC ==========" -ForegroundColor Green
cmake --build build --config Release --parallel

Write-Host "========== Phase 3: Packaging Performance Monitor SKSE Plugin ==========" -ForegroundColor Green
$MOD_DIR = "$PSScriptRoot/Mod_Package"
if (Test-Path $MOD_DIR) { Remove-Item -Recurse -Force $MOD_DIR }
New-Item -ItemType Directory -Force "$MOD_DIR/SKSE/Plugins" | Out-Null

# Copy compiled plugin dll, pdb and default ini configuration
Copy-Item "$PSScriptRoot/dist/SKSE/Plugins/skyrim_perf_monitor.dll" "$MOD_DIR/SKSE/Plugins/"
if (Test-Path "$PSScriptRoot/dist/SKSE/Plugins/skyrim_perf_monitor.pdb") {
    Copy-Item "$PSScriptRoot/dist/SKSE/Plugins/skyrim_perf_monitor.pdb" "$MOD_DIR/SKSE/Plugins/"
}
Copy-Item "$PSScriptRoot/skyrim_perf_monitor.ini" "$MOD_DIR/SKSE/Plugins/"

Write-Host "==========================================================================" -ForegroundColor Green
Write-Host "Build and packaging completed successfully!" -ForegroundColor Green
Write-Host "The ready-to-use performance monitor is located at: $MOD_DIR" -ForegroundColor Yellow
Write-Host "You can drop this 'Mod_Package' directory directly into your Mod Organizer 2" -ForegroundColor Yellow
Write-Host "mods/ folder to activate the general Skyrim Performance Monitor!" -ForegroundColor Yellow
Write-Host "==========================================================================" -ForegroundColor Green
