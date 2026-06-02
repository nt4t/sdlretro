## Goal
Remove X11 Shared Memory (XShm) video backend. Add Linux framebuffer (/dev/fb0) renderer.

## Changes Made
- Removed `src/drivers/sdl2/x11_shm_video.cpp` and `x11_shm_video.h`
- Removed `docs/superpowers/specs/2026-05-31-x11-shm-video-design.md`
- Removed X11_SHM conditional compilation from `src/drivers/sdl2/sdl2_impl.cpp`
- Removed `SDLRETRO_X11_SHM` CMake option from `src/CMakeLists.txt`
- Removed X11 dependencies from `src/drivers/sdl2/CMakeLists.txt`
- Removed `process_x11_events()` call from `sdl2_impl::process_events()`

## Relevant Files
- `src/drivers/sdl2/sdl2_impl.cpp`: Simplified to always use OpenGL backend
- `src/CMakeLists.txt`: Removed `SDLRETRO_X11_SHM` option and X11 detection
- `src/drivers/sdl2/CMakeLists.txt`: Removed x11_shm_video sources and X11 link deps
