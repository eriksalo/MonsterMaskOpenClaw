# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Monster M4SK animated eyes firmware with runtime mood switching. Drives two 240x240 ST7789 displays on an Adafruit Monster M4SK (SAMD51G19A, 120MHz, 192KB RAM, 496KB Flash). Nine moods (happy, angry, sad, scared, sleepy, suspicious, surprised, love, crazy) are switchable at runtime via serial commands, designed for future MQTT bridging from OpenClaw.

## Build Commands

```bash
# Build firmware
pio run

# Build and upload to connected M4SK
pio run --target upload

# Clean build artifacts
pio run --target clean

# Serial monitor (115200 baud)
pio device monitor --baud 115200
```

No test framework is configured. Verification is done on hardware via serial mood commands.

## Architecture

### Rendering Pipeline

The eye renderer in `M4_Eyes.ino` uses **column-at-a-time DMA rendering** — each `loop()` iteration renders ONE column for ONE eye, alternating between eyes. Display uses ROTATION 3: (0,0) at bottom-left, +X up, +Y right. Double-buffered column structs (A/B) overlap DMA transfer with pixel computation.

Key rendering state per eye: `colNum` (current column, counts down from 239), `colIdx` (A/B buffer toggle), `dma_busy`/`column_ready` flags.

### Global Variable System

`globals.h` uses `GLOBAL_VAR`/`GLOBAL_INIT` macros. In `M4_Eyes.ino` (where `INIT_GLOBALS` is defined), these expand to definitions with initializers. In all other `.cpp` files, they expand to `extern` declarations. All new globals must use this pattern.

### Mood Reload System

Three files cooperate for runtime mood switching:
- **`user.cpp`** — Non-blocking serial parser in `user_loop()`. Receives `MOOD:<name>\n` commands, sets `reloadRequested = true` and populates `reloadConfigPath`.
- **`reload.cpp`** — `reloadEyeConfig()` waits for DMA idle, reloads config/textures/eyelids. Maintains an 8-entry texture cache to avoid exhausting the finite 512KB internal flash (textures are written via `writeDataToFlash()` which cannot reclaim space).
- **`M4_Eyes.ino`** — Checks `reloadRequested` after `user_loop()` returns, calls `reloadEyeConfig()`, then returns to restart the render loop.

**Critical constraint:** `eyeRadius`, `irisRadius`, and `slitPupilRadius` must be identical across all moods (currently 125, 85, 0). Changing these would require regenerating the ~125KB polar lookup tables, which is too slow and risks malloc fragmentation.

### Config & Asset Loading (`file.cpp`)

`loadConfig()` parses JSON `config.eye` files using `StaticJsonDocument<2048>`. `loadTexture()` reads 16-bit BMP from QSPI filesystem, byteswaps for SPI endianness, and copies to internal flash. `loadEyelid()` converts 1-bit 240x240 BMP into min/max column arrays.

Mood assets live under `M4_Eyes/eyes/moods/<name>/` with `config.eye`, `upper.bmp`, `lower.bmp`, and optionally a custom `iris.bmp`. Moods can reference shared textures from other eye sets (e.g., `big_blue/iris_blue.bmp`).

### Texture Formats

- **Iris/sclera**: 16-bit RGB565 BMP, polar-mapped (X=angle 0-360°, Y=radius)
- **Eyelids**: 240x240 1-bit monochrome BMP (white=open region)
- Both must be bottom-to-top row order with 4-byte row padding per BMP spec

## Serial Protocol

At 115200 baud, newline-terminated:
```
MOOD:<name>    # Switch mood (happy|angry|sad|scared|sleepy|suspicious|surprised|love|crazy)
MOOD:list      # List available moods
STATUS         # Current mood and frame info
```

## Asset Generation Tools

```bash
# Generate eyelid BMPs for all moods
python tools/generate_eyelids.py

# Generate love iris texture
python tools/generate_love_iris.py [output_path]
```

## PlatformIO Build Quirks

- `src_dir = M4_Eyes` is required because the .ino sketch lives there (not in `src/`)
- `-DUSE_TINYUSB` is mandatory for the USB stack
- Explicit `-I` flags for SPI and Wire framework library paths are needed because TinyUSB 3.x transitively requires `SPI.h` from within the ZeroDMA library context
- `lib_ldf_mode = deep+` and `lib_compat_mode = off` are needed for transitive dependency resolution
- Disabled `user_*.cpp` variants must be excluded via `build_src_filter` — even `#if 0` files get their includes scanned by LDF
- `USBHost` library must be ignored (broken on SAMD51, only needed by excluded `user_hid.cpp`)
