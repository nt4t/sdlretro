# X11 Shm Video Backend - Implementation Plan

## Goal
Add an X11 Shared Memory (XShm) video rendering backend for sdlretro on Linux/X11 systems. Opt-in via `SDLRETRO_X11_SHM` build option. Falls back to OpenGL if unavailable.

## Phases

### Phase 1: Build System Setup
- [x] Add `SDLRETRO_X11_SHM` option to `src/CMakeLists.txt`
- [x] Add X11/XShm dependency detection in CMake
- [x] Add `x11_shm_video.cpp` to SDL2 driver sources
- [x] Add compile definition `SDLRETRO_X11_SHM`

### Phase 2: X11 Shm Video Backend (x11_shm_video.h/cpp)
- [x] Create `x11_shm_video.h` — header with class declaration
- [x] Create `x11_shm_video.cpp` — implementation
  - [x] `init_video()` — XOpenDisplay, XCreateSimpleWindow, XShmCreateImage
  - [x] `render()` — XShmPutImage for frame delivery
  - [x] `frame_render()` — no-op (synchronous)
  - [x] `flip()` — no-op
  - [x] `clear()`, `fill_rectangle()` — X11 drawing primitives
  - [x] `draw_text()` — X11 bitmap text
  - [x] `gui_predraw()` — prepare for overlay
  - [x] `get_resolution()`, `window_resized()`, `game_resolution_changed()`
  - [x] Fallback to OpenGL if X11/XShm unavailable

### Phase 3: Driver Integration
- [x] Modify `sdl2_impl.h` — conditional video class selection
- [x] Modify `sdl2_impl.cpp` — conditional window creation (X11 vs SDL2)
- [x] Wire up input events for X11 window (SDL already handles this)

### Phase 4: Testing & Verification
- [ ] Build with `SDLRETRO_X11_SHM=ON` on Linux/X11
- [ ] Build with `SDLRETRO_X11_SHM=OFF` (no regression)
- [ ] Build on non-Linux platforms (no regression)
- [ ] Verify OpenGL fallback when X11 unavailable

## Decisions (from design doc)
- Linux/X11 only
- XRGB8888 pixel format only
- Full GUI overlay via X11 primitives
- Bitmap font fallback (no FreeType)
- No integer scaling, no custom shaders
- Synchronous rendering (XShmPutImage blocks)

## Errors Encountered
| Error | Attempt | Resolution |
|-------|---------|------------|
| | | |
