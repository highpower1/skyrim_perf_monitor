# Skyrim Performance Monitor (skyrim_perf_monitor)

A lightweight, asynchronous performance monitoring SKSE plugin for Skyrim Special Edition (SE), Anniversary Edition (AE), and Skyrim VR.

The plugin hooks into Skyrim's main loop using CommonLibSSE to collect frame times, engine deltas, active actor counts, and process memory usage (Working Set Size). Collected metrics are buffered in-memory and flushed to a CSV file on a background thread to prevent disk I/O operations from impacting frame delivery.

This plugin is designed to benchmark and analyze performance differences under various game states, measuring the cost of high NPC density, or tracking memory leaks.

It was originally designed for benchmarking Jolt HDT-SMP(https://github.com/highpower1/hdtSMP64).

---

## Features

- Frame Time Measurement: Measures frame time using std::chrono::high_resolution_clock with microsecond precision.
- Active Actor Tracking: Queries Skyrim's internal RE::ProcessLists to log the number of active, high-priority NPCs processing in the current cell.
- Memory Tracking: Logs the process's physical memory usage (Working Set Size) via Win32 PSAPI.
- Engine Delta Logging: Records both unscaled (Engine_RealDelta) and scaled (Engine_Delta) game clock deltas.
- Asynchronous Disk I/O: Buffers metrics in-memory and writes batches (default 1000 frames) on a separate background thread to avoid game-loop stuttering.
- Configuration: Configurable via a standard .ini file.

---

## Configuration (skyrim_perf_monitor.ini)

The configuration file is located under Data/SKSE/Plugins/. It supports the following settings:

```ini
[Settings]
; Number of initial startup/loading frames to skip before logging begins
WarmupFrames = 100

; Number of frames to buffer in memory before flushing to disk
FlushInterval = 1000

; Toggle physical memory usage logging (1 = Enabled, 0 = Disabled)
LogRAM = 1

; Toggle active high-priority actor (NPC) count logging (1 = Enabled, 0 = Disabled)
LogHighActors = 1

; Filename of the output CSV log. Saved under "My Games/Skyrim Special Edition/SKSE/"
OutputFileName = skyrim_perf_log.csv
```

---

## CSV Log Format

The log is saved to `My Games/Skyrim Special Edition/SKSE/skyrim_perf_log.csv` (or the configured filename) in a comma-separated format:

```csv
FrameIndex,Engine_Delta_ms,Engine_RealDelta_ms,Measured_FrameTime_ms,Measured_FPS,RAM_MB,High_Actors
101,16.6667,16.6667,16.5541,60.0453,2442.23,31
102,16.6667,16.6667,18.0532,55.3918,2443.59,31
```

---

## Installation & Usage

1. Copy the contents of the `Mod_Package` folder (or install via a mod manager like MO2 or Vortex).
2. Verify that `skyrim_perf_monitor.dll` and `skyrim_perf_monitor.ini` are located under `Data/SKSE/Plugins/`.
3. Launch Skyrim and run the benchmark.
4. Upon exiting the game, the finalized CSV log will be saved under `My Games/Skyrim Special Edition/SKSE/`.

---

## Build Instructions

This project is built using C++20 and CMake.

### Dependencies
All dependencies are resolved via vcpkg in manifest mode:
- CommonLibSSE (via Address Library)
- xbyak
- spdlog
- fmt
- directxtk
- rapidcsv

### Compilation
1. Ensure MSVC C++ and CMake are installed.
2. Set the `VCPKG_ROOT` environment variable to your vcpkg installation path.
3. Execute the build script in PowerShell:
   ```powershell
   ./build_perf.ps1
   ```
4. The output binaries and directory structure will be generated in `Mod_Package/`.

---

## Credits

- Built using CommonLibSSE and SKSE.
