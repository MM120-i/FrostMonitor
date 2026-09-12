# FrostMonitor

FrostMonitor displays your computer's CPU temperature, GPU temperature, and in-game FPS on the OLED screen of a compatible SteelSeries keyboard.

It runs quietly in the Windows system tray and sends data to SteelSeries GameSense on your local computer.

## What It Displays

The OLED screen can show lines such as:

```text
CPU n/a | 8%
GPU 40.0 C | 3%
FPS 144
```

The CPU temperature may show `n/a` when Windows does not expose a supported CPU temperature sensor. CPU utilization and GPU readings can still work normally.

## Requirements

- Windows 10 or Windows 11, 64-bit
- A SteelSeries keyboard with an OLED screen
- SteelSeries GG or SteelSeries Engine with GameSense enabled
- RivaTuner Statistics Server (RTSS) for FPS data
- An NVIDIA driver for GPU temperature and utilization data

RTSS and an NVIDIA GPU are optional. FrostMonitor continues running when either one is unavailable, but the corresponding data will not be displayed.

## Quick Start

1. Start SteelSeries GG and make sure your keyboard is connected.
2. Start RTSS if you want FPS data.
3. Build the release version or use an existing release executable.
4. Start FrostMonitor from the project directory:

```powershell
.\build\release\Release\FrostMonitor.exe --tray config\config.json
```

The `--tray` option hides the console window. FrostMonitor will remain available from the system tray.

Right-click the tray icon to:

- Pause or resume updates
- Open the configuration file
- Exit FrostMonitor

Only one FrostMonitor instance can run at a time.

## Building From Source

### Build Requirements

- Visual Studio with the C++ desktop workload
- CMake 3.27 or newer
- vcpkg
- The dependencies listed in `vcpkg.json`

The CMake presets currently expect vcpkg at `C:/vcpkg`. Change `CMakePresets.json` if vcpkg is installed elsewhere.

### Build and Run

From PowerShell in the project directory:

```powershell
.\dev.bat build release
.\build\release\Release\FrostMonitor.exe --tray config\config.json
```

For development builds with AddressSanitizer enabled:

```powershell
.\dev.bat build debug
.\dev.bat test debug
```

The debug executable may require the AddressSanitizer runtime DLL beside it. The build process copies it automatically when available.

## Command-Line Options

```text
--tray             Hide the console window and run from the system tray
--demo             Use sample CPU and GPU values instead of hardware sensors
--check-sensors    Read CPU and GPU sensors once and print the results
```

An optional configuration path can be supplied after the options:

```powershell
.\build\release\Release\FrostMonitor.exe --tray config\config.json
```

## Configuration

The default configuration is `config/config.json`.

```json
{
  "app": { "name": "FrostMonitor" },
  "polling_interval_ms": 1000,
  "autostart": false,
  "gamesense": {
    "address": "",
    "discovery_file": "C:/ProgramData/SteelSeries/SteelSeries Engine 3/coreProps.json",
    "register_game": true
  },
  "events": {
    "cpu": { "name": "CPU_STATS", "min": 0, "max": 100 },
    "gpu": { "name": "GPU_STATS", "min": 0, "max": 100 },
    "fps": { "name": "FPS_STATS", "min": 0, "max": 300 }
  },
  "logging": {
    "level": "debug",
    "dir": "logs",
    "max_bytes": 5242880,
    "max_files": 5
  }
}
```

### Main Settings

| Setting | Description |
| --- | --- |
| `polling_interval_ms` | How often sensor values are read. The default is 1000 milliseconds. |
| `autostart` | Adds or removes FrostMonitor from the current Windows user's startup apps. |
| `gamesense.register_game` | Enables or disables communication with SteelSeries GameSense. |
| `gamesense.address` | GameSense address. Leave empty to use SteelSeries discovery. |
| `gamesense.discovery_file` | Location of SteelSeries Engine's `coreProps.json` file. |
| `logging.level` | Logging level such as `debug`, `info`, `warn`, or `error`. |
| `logging.dir` | Directory where log files are written. |

When `autostart` is `true`, FrostMonitor registers itself to start hidden in the system tray when you sign in to Windows.

## FPS Setup

FrostMonitor reads FPS from RTSS using its local shared-memory interface.

1. Install and start RivaTuner Statistics Server.
2. Start the game you want to monitor.
3. Make sure RTSS detects the game.
4. Keep the game as the foreground window.
5. Start FrostMonitor.

If RTSS is not running, the log will contain:

```text
FPS Sensor: RTSS shared memory not found
```

This is not fatal. CPU and GPU monitoring will continue.

## Troubleshooting

### The OLED screen stays blank

- Confirm SteelSeries GG is running.
- Confirm GameSense is enabled.
- Confirm the keyboard supports OLED screen handlers.
- Check the FrostMonitor log for `GameSense LIVE`.
- Restart SteelSeries GG and FrostMonitor if an older registration is stuck.

### FPS is missing

- Start RTSS before FrostMonitor.
- Confirm the game appears in RTSS.
- Make sure the game is the foreground window.
- Check that the log no longer reports missing RTSS shared memory.

### CPU temperature shows `n/a`

This means the current Windows sensor interface does not expose a supported CPU temperature zone. CPU utilization and GPU values are independent of this reading.

### GPU values are missing

The current implementation uses NVIDIA's NVML interface. Confirm that an NVIDIA driver is installed. GPU monitoring is optional and does not prevent the rest of the application from running.

### The application says another instance is running

FrostMonitor intentionally allows only one running instance. Close the existing tray instance before starting another copy.

## Developer Commands

```powershell
.\dev.bat build debug       # Build the debug configuration
.\dev.bat build release     # Build the release configuration
.\dev.bat test debug        # Run the debug test suite
.\dev.bat test release      # Run the release test suite
.\dev.bat lint              # Run clang-tidy
.\dev.bat clean             # Remove build directories
```

The project uses C++23, MSVC warning level 4, warnings-as-errors, clang-tidy, and Catch2 tests.

## Project Layout

```text
include/frostmonitor/   Public C++ interfaces
src/                    Application and monitoring implementation
tests/                  Automated tests
config/config.json      Default user configuration
dev.bat                 Build, test, lint, and clean commands
```

## How It Works

1. CPU and GPU sensors are read locally on a timer.
2. RTSS provides the FPS value for the foreground game.
3. FrostMonitor sends the three values to the local SteelSeries GameSense service.
4. GameSense renders the values on the keyboard's OLED screen.

FrostMonitor does not need a cloud account or an internet connection for sensor and OLED updates.
