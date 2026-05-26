# Skyrim Performance Monitor (`skyrim_perf_monitor`)

An extremely lightweight, zero-overhead, asynchronous benchmarking SKSE plugin for **Skyrim Special Edition (SE), Anniversary Edition (AE), and Skyrim VR**.

Unlike standard frame capture utilities, this plugin hooks directly into Skyrim's main loop and internal managers using the CommonLibSSE Address Library. It gathers deep, high-precision metrics (including engine deltas, active actor counts, and Working Set RAM) and flushes them to a CSV file asynchronously on a background thread to guarantee absolutely zero impact on gameplay frame delivery.

This is an invaluable scientific benchmark tool for Skyrim players, modders, and developers to objectively compare performance impacts of physics engines (e.g. FSMP vs JSMP), NPC density mods, and monitor memory leak profiles over long play sessions.

---

## ✨ Features

- ⏱️ **High-Precision Frame Time Measurement**: Captures real-world frame time using `std::chrono::high_resolution_clock` with microsecond precision.
- 📉 **Jitter & Stutter Diagnostics**: Ideal for calculating true `1% Low` and `0.1% Low` FPS to measure gameplay smoothness and frame spikes.
- 👥 **Active Actor Tracking**: Dynamically queries Skyrim's internal `RE::ProcessLists` to count active, high-priority NPCs currently processing in the player's immediate cell.
- 💾 **Process RAM Tracking**: Logs the game's actual physical memory usage (Working Set Size) in real-time.
- ⚙️ **Dual-Engine Delta Logging**: Records both unscaled (`Engine_RealDelta`) and scaled (`Engine_Delta`) game clock deltas.
- 🧵 **Asynchronous double-buffering**: Buffers metrics in-memory and writes batches (default 1000 frames) on a dedicated background thread, eliminating any possibility of disk I/O freezes or framerate stuttering during gameplay.
- 🔧 **Dynamic Configuration**: Fully customizable behavior via an `.ini` file.

---

## ⚙️ Configuration (`skyrim_perf_monitor.ini`)

A default `.ini` file is packaged under `Data/SKSE/Plugins/`. You can customize the benchmarking metrics on the fly:

```ini
[Settings]
; Number of initial startup/loading frames to skip to prevent benchmark averages from warping
WarmupFrames = 100

; In-memory frame buffer size before flushing metrics to disk (0 I/O impact during gameplay)
FlushInterval = 1000

; Log Working Set RAM usage in Megabytes (1 = Enabled, 0 = Disabled)
LogRAM = 1

; Log active high-priority actor (NPC) count in the vicinity (1 = Enabled, 0 = Disabled)
LogHighActors = 1

; Target filename. The CSV is saved under "My Games/Skyrim Special Edition/SKSE/"
OutputFileName = skyrim_perf_log.csv
```

---

## 📊 CSV Log Format

The benchmark logs are saved to `My Games/Skyrim Special Edition/SKSE/skyrim_perf_log.csv` (or the configured filename) and structured as a simple comma-separated layout that can be imported directly into Excel, Google Sheets, or Python:

```csv
FrameIndex,Engine_Delta_ms,Engine_RealDelta_ms,Measured_FrameTime_ms,Measured_FPS,RAM_MB,High_Actors
101,16.6667,16.6667,16.5541,60.0453,2442.23,31
102,16.6667,16.6667,18.0532,55.3918,2443.59,31
```

---

## 🚀 Installation & Usage

1. Copy the contents of the `Mod_Package` folder (or install it via MO2/Vortex as a mod).
2. Ensure the `skyrim_perf_monitor.dll` and `skyrim_perf_monitor.ini` are correctly placed under `Data/SKSE/Plugins/`.
3. Launch Skyrim and load your target benchmark scene.
4. Play, test, or stand still for your desired duration.
5. Exit Skyrim. The CSV log will be closed and finalized in your Documents folder under `My Games/Skyrim Special Edition/SKSE/`.

---

## 🛠️ Build Requirements & Guide

This project is built using C++20 and standard CMake.

### Dependencies
All dependencies are automatically resolved in manifest mode using `vcpkg`:
- `CommonLibSSE` (via Address Library)
- `xbyak` (built-in SKSE trampoline)
- `spdlog` & `fmt`
- `directxtk` & `rapidcsv`

### Compilation Steps
1. Make sure you have Microsoft Visual Studio (MSVC C++) and CMake installed.
2. Configure your `VCPKG_ROOT` environment variable.
3. Open a PowerShell console and run:
   ```powershell
   ./build_perf.ps1
   ```
4. The script will automatically trigger `vcpkg` package acquisition, compile the source in Release configuration, and structure a deployable mod package under `Mod_Package/`.

---

## 👤 Credits & Author

- **Author**: `highpower1`
- Built using `CommonLibSSE` and `SKSE`.
