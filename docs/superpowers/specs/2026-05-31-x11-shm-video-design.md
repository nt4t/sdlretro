# X11 Shm Video Backend Design

**Date:** 2026-05-31  
**Status:** Approved

## Overview

Add an X11 Shared Memory (XShm) video rendering backend for sdlretro on Linux/X11 systems. This provides lower-latency frame delivery by writing pixel data directly into X server memory, bypassing the OpenGL pipeline. The existing SDL2 OpenGL backend remains the default; X11 Shm is opt-in via a build option.

## Scope

- Linux/X11 only
- XRGB8888 pixel format
- Full GUI overlay support (in-game menu, OSD messages)
- Minimal viable: no integer scaling, no custom shaders
- Falls back to OpenGL backend if X11 Shm unavailable

## Architecture

```
sdl2_impl (SDL2 driver)
    ├── video: sdl2_video (OpenGL) — existing, default
    └── video: x11_shm_video (X11 Shm) — new, opt-in
    ├── audio: sdl2_audio — unchanged
    ├── input: sdl2_input — unchanged
    └── events: SDL_PollEvent — unchanged
```

## Components

### 1. `x11_shm_video.cpp` / `x11_shm_video.h`

New source files implementing a `video_base` subclass using X11 + XShm.

**Key state:**
- `Display *display` — X11 display connection
- `Window window` — X11 window handle
- `ShmSegment *shm` — shared memory segment(s) for frame buffers
- `int width`, `int height` — current resolution
- `bool drawn` — whether a frame was drawn (for auto frameskip)

**Key methods:**
- `init_video(int w, int h)` — open display, create window, allocate Shm segment
- `render(const void *data, int w, int h, size_t pitch)` — `XShmPutImage()`
- `frame_render()` — no-op (XShmPutImage is synchronous)
- `flip()` — no-op
- `clear()`, `fill_rectangle()`, `draw_text()` — X11 drawing primitives
- `gui_predraw()` — prepare overlay surface before menu draw

### 2. Window Creation

On Linux with `SDLRETRO_X11_SHM` enabled, `sdl2_impl` creates an X11 window directly:
- `XOpenDisplay(nullptr)`
- `XCreateSimpleWindow()` for the main window
- `XSelectInput()` for key/mouse events
- `XMapWindow()` to show the window

On non-Linux platforms, falls back to existing SDL2 window creation.

### 3. Shared Memory Allocation

- Allocate a shared memory segment via `shmget()` + `shmat()`
- Segment size = `width * height * 4` (XRGB8888 = 4 bytes per pixel)
- Attach to X11 via `XShmCreateImage()`
- `XShmPutImage()` pushes each frame (synchronous, blocks until X server consumes)

### 4. GUI Overlay

The menu overlay is rendered via X11 primitives on top of the Shm surface:
- `XSetForeground()` for color
- `XFillRectangle()` for rectangles
- `XDrawString()` / `XDrawImageString()` for text (bitmap font fallback)

Overlay is drawn after the game frame in `frame_render()`, before `flip()`.

### 5. Fallback Chain

```
SDLRETRO_X11_SHM enabled?
  ├── No → use sdl2_video (OpenGL) — existing behavior
  └── Yes → try x11_shm_video
              ├── XOpenDisplay fails → use sdl2_video (OpenGL)
              └── XShm available → use x11_shm_video
```

## Build Configuration

### CMake option (src/CMakeLists.txt)
```cmake
option(SDLRETRO_X11_SHM "Enable X11 Shared Memory backend (Linux only)" OFF)
```

### CMake dependencies (src/drivers/CMakeLists.txt or external/CMakeLists.txt)
```cmake
if(SDLRETRO_X11_SHM)
    find_package(X11 REQUIRED COMPONENTS Xext)
endif()
```

### Compile definitions (src/CMakeLists.txt)
```cmake
if(SDLRETRO_X11_SHM)
    target_compile_definitions(sdlretro PRIVATE SDLRETRO_X11_SHM)
    target_link_libraries(sdlretro ${X11_LIBRARIES} ${X11_XSHM_LIB})
endif()
```

### Source files (src/drivers/sdl2/CMakeLists.txt)
```cmake
if(SDLRETRO_X11_SHM)
    list(APPEND SDL2_VIDEO_SOURCES x11_shm_video.cpp)
endif()
```

## File Changes

| File | Change |
|------|--------|
| `src/CMakeLists.txt` | Add `SDLRETRO_X11_SHM` option, link X11 libs |
| `src/drivers/sdl2/CMakeLists.txt` | Add `x11_shm_video.cpp` to sources |
| `src/drivers/sdl2/x11_shm_video.h` | **New** — header for X11 Shm video backend |
| `src/drivers/sdl2/x11_shm_video.cpp` | **New** — implementation |
| `src/drivers/sdl2/sdl2_impl.h` | Add X11 window creation, conditional video class |
| `src/drivers/sdl2/sdl2_impl.cpp` | Conditional window/video creation based on backend |

## Constraints

- **Linux/X11 only** — no Wayland, no macOS, no Windows support
- **XRGB8888 only** — the most common format, covers most libretro cores
- **No integer scaling** — XShm works best at native resolution
- **No custom shaders** — rendering is pixel-copy, not GPU-accelerated
- **Bitmap font fallback** — FreeType not used; menu text uses X11 bitmap fonts
- **Synchronous rendering** — `XShmPutImage` blocks; no double-buffering

## Success Criteria

1. `cmake -DSDLRETRO_X11_SHM=ON` configures successfully on Linux with X11/XShm
2. Game frames render correctly via XShmPutImage
3. In-game menu overlay renders on top of game frames
4. Falls back to OpenGL if X11 Shm unavailable
5. No regressions to existing SDL2 OpenGL backend
