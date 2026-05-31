# sdlretro

A lightweight libretro frontend written in C++ for SDL, optimized for retro handheld devices like GCW-Zero and RG-350.

## Overview

sdlretro loads libretro cores dynamically and provides a simple, easy-to-maintain frontend with SDL-based rendering, audio, and input. It targets resource-constrained platforms while remaining functional on desktop systems.

**Version:** 0.1.0  
**Languages:** C11, C++17  
**License:** See [LICENSE](LICENSE)

## Features

- Dynamic loading of libretro cores (`.so`/`.dll`)
- Core selection menu when multiple cores support the same ROM extension
- ZIP ROM support (up to 2 files: one ROM + one dir/readme)
- In-game menu (F1) with Global Settings, Core Settings, Input Settings, Language, Reset, and Exit
- Configuration via JSON config file
- Internationalization (English, Chinese Simplified)
- Audio resampling with libsamplerate
- SRAM/RTC save management
- OpenGL rendering (SDL2) or software rendering (SDL1)

## Build

### Requirements

- CMake 3.0+
- SDL 1.2 or SDL2
- FreeType (default on Linux/macOS) or stb_truetype (default on Windows)
- libcurl (optional, for core downloader on non-Windows)

### Desktop (Linux/macOS/Windows)

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DSDLRETRO_FRONTEND=SDL2 ..
make
```

### GCW-Zero / RG350

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DMODEL=gcw0 ..
make
./make_opk.sh          # Lite version
FULL=1 ./make_opk.sh   # Full version (with cores)
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `SDLRETRO_FRONTEND` | SDL2 (SDL1 on OpenDingux) | SDL driver version |
| `SDLRETRO_USE_STB_TRUETYPE` | ON on Windows, OFF on Linux/macOS | Use stb_truetype instead of FreeType |
| `SDLRETRO_CORE_DOWNLOADER` | ON (OFF on Windows) | Enable libretro core downloader (requires libcurl) |
| `SDLRETRO_USE_STATIC_CRT` | ON on Windows, OFF on Linux | Static C runtime linking |

## Running

```bash
# Auto-detect core from ROM extension
./sdlretro rom.gb

# Specify core explicitly
./sdlretro -L mupen64plus_libretro.so rom.n64
```

### Configuration

Config file: `~/.sdlretro/cfg/sdlretro.json` (or `./cfg/sdlretro.json` on Windows)

```json
{
    "res_w": 1280,
    "res_h": 720,
    "scale": 2,
    "fullscreen": false,
    "integer_scaling": false,
    "linear": true,
    "mono_audio": false,
    "sample_rate": 0,
    "resampler_quality": 0,
    "scaling_mode": 0,
    "save_check": 0,
    "language": 0
}
```

### Key Bindings (SDL2, non-GCW)

| Button | Key |
|--------|-----|
| A | L |
| B | K |
| X | I |
| Y | J |
| SELECT (menu) | F1 |
| START | V |
| L | Q |
| R | E |
| L2 | 1 |
| R2 | 3 |
| L3 | Z |
| R3 | X |
| D-Pad / Analog | W A S D |

## Architecture

```
main.cpp
   └── gui::ui_host ──> gui::sdl_menu ──> gui::sdl_elem
         │
         └── drivers::driver_base ──> driver_common (audio/video/input/font/throttle)
                    │
                    └── src/libretro/ (core, cfg, variables, helper, i18n, downloader, vfs)
                               │
                               └── external/ (fmt, json, glad, miniz, stb, libsamplerate, xxhash, cpuid)
```

### Directory Structure

| Directory | Purpose |
|-----------|---------|
| `src/` | Main source code |
| `src/main.cpp` | Entry point, CLI args, emulation loop |
| `src/drivers/` | Platform drivers (SDL1/SDL2) for audio, video, input, font |
| `src/gui/` | Menu system with element types (static, boolean, values, input) |
| `src/libretro/` | Core loading, config, variables, I18n, VFS, downloader |
| `src/util/` | Structured logging (fmt-powered) |
| `external/` | Third-party dependencies (git submodules) |
| `lang/` | Internationalization files (en-US.json, zh-CN.json) |
| `data/` | Application resources (icon) |
| `opk/` | GCW-Zero packaging scripts |
| `cmake/` | Cross-compilation toolchain files |

## Dependencies

| Library | Purpose |
|---------|---------|
| [fmt](https://github.com/fmtlib/fmt) | Fast C++ formatting |
| [nlohmann/json](https://github.com/nlohmann/json) | JSON parsing/serialization |
| [glad](https://github.com/Dav1dde/glad) | OpenGL loader |
| [miniz](https://github.com/richgel999/miniz) | ZIP/TAR compression |
| [stb](https://github.com/nothings/stb) | Single-file image/font libraries |
| [libsamplerate](https://github.com/erikd/libsamplerate) | Audio sample rate conversion |
| [xxHash](https://github.com/Cyan4973/xxHash) | Fast hash function |
| [cpuid](https://github.com/steinwurf/cpuid) | CPU feature detection |

## Credits

- [libretro](https://github.com/libretro/libretro-common)
- [JSON for Modern C++](https://github.com/nlohmann/json)
- [miniz](https://github.com/richgel999/miniz)
- [stb](https://github.com/nothings/stb)
- [glad](https://github.com/Dav1dde/glad)
- [libsamplerate](https://github.com/erikd/libsamplerate)
- [xxHash](https://github.com/Cyan4973/xxHash)
- [cpuid](https://github.com/steinwurf/cpuid)
- [wingetopt](https://github.com/alex85k/wingetopt)
- [fmtlib](https://github.com/fmtlib/fmt)
