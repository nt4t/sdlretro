## Goal
Add X11 Shared Memory (XShm) video rendering backend for sdlretro on Linux/X11 systems, with OpenGL fallback. Fix SDL1 backend menu/input issues.

## Constraints & Preferences
- Linux/X11 only, no Wayland/macOS/Windows support
- XRGB8888 pixel format only
- Configurable via build option `SDLRETRO_X11_SHM` (default OFF)
- Full GUI overlay support via X11 primitives
- Minimal viable: no integer scaling, no custom shaders
- Bitmap font fallback (no FreeType)
- Synchronous rendering (XShmPutImage blocks)
- Clean separation: new x11_shm_video.cpp/h, not modifying existing sdl2_video
- SDL1 backend uses built-in bitmap font for all text rendering

## Progress
### Done
- Created design spec: `docs/superpowers/specs/2026-05-31-x11-shm-video-design.md`
- Added `SDLRETRO_X11_SHM` CMake option with X11/XShm dependency detection
- Created `src/drivers/sdl2/x11_shm_video.h` — header with video_base interface
- Created `src/drivers/sdl2/x11_shm_video.cpp` — full X11 Shm implementation
- Modified `src/drivers/sdl2/sdl2_impl.cpp` — conditional backend selection (X11 Shm → OpenGL fallback)
- Fixed CMake detection for CMake 3.10+ new FindX11 (X11_XSHM_FOUND → X11_Xext_FOUND)
- Added SDLRETRO_X11_SHM compile definition to driver_sdl2 target (was only on sdlretro target)
- Added X11/XShm headers to header file (`<X11/Xlib.h>`, `<X11/extensions/XShm.h>`)
- Fixed `ShmSeg` type: replaced cast with proper `XShmSegmentInfo` struct
- Added `game_max_width`/`game_max_height` members to class
- Added X11 event processing (`process_x11_events`) with key/mouse callback mechanism
- Added window title via `XStoreName`
- Fixed `XEvent.button` → `XEvent.xbutton.button`
- Fixed `XShmCreateImage` width/height params: was `width*4, 32`, now `width, height`
- Fixed `XShmCreateImage` to use `&shm_info` parameter so X server can verify shared memory
- Simplified `game_resolution_changed`: keep initial 640x480 shm_image, don't recreate
- Added Alt+Enter fullscreen toggle to SDL1 backend (preserves SDL_FULLSCREEN flag)
- Added FPS counter toggleable with F3 key (uses built-in pixel font)
- Added F1 key to SDL1 backend for menu toggle
- SDL1 backend now always uses `draw_text_pixel()` for bitmap font (TTF path removed)
- Fixed `get_text_width_and_height` to always use pixel font (was using TTF path causing highlight mismatch)
- Removed hardcoded `y -= 16` offset from `draw_text_pixel` (caused menu highlight misalignment)
- Fixed F1/ESC in menu: now closes menu instead of quitting (added `get_menu_button_pressed()` to driver_base)
- SDL1 default keymap now populates for unmapped keys (not skipped if config exists)
- SDL1 menu keymap: W/S=up/down, A/D=left/right, L=select, K=back, Q=page up, E=page down, 1=first, 3=last
- Menu input mode set to `mode_menu` when menu opens

### In Progress
- Phase 4: Testing & Verification — XShmPutImage still returns 0 (failure) despite all fixes applied

### Blocked
- None

## Key Decisions
- Approach A: Separate X11 Shm video backend (clean separation from OpenGL)
- Build option `SDLRETRO_X11_SHM` default OFF, requires X11/XShm libraries
- Fallback chain: X11 Shm → OpenGL if XShm unavailable or on non-Linux
- XRGB8888 only format, bitmap font fallback, no integer scaling
- X11 window management (not SDL window) when X11 Shm enabled
- Reuse initial 640x480 shm_image instead of reallocating on resolution change
- `XShmCreateImage` now uses `&shm_info` so X server can verify shared memory
- SDL1: always use pixel font via `draw_text_pixel()`, bypass TTF
- SDL1: input mode switches to `mode_menu` when menu opens
- SDL1: default keymap populated for any unmapped keys (config file doesn't override)
- F1/ESC: single-press toggle (first press sets `menu_button_pressed`, second press triggers menu/menu-close)

## Next Steps
1. Rebuild and test X11 Shm backend with all fixes applied
2. Verify XShmPutImage succeeds with `&shm_info` fix
3. Verify SDL1 menu navigation works with W/S keys
4. If XShmPutImage still fails, investigate XShmPutImage error codes or try XPutImage fallback
5. Test in-game menu overlay rendering with bitmap font

## Critical Context
- sdlretro architecture: `video_base` abstract interface, SDL2 backend uses OpenGL (sdl2_video.cpp)
- XShm API: `XShmQueryExtension`, `XShmCreateImage`, `XShmPutImage`, `shmget`/`shmat`/`shmdt`/`shmctl`
- X11 drawing primitives: `XSetForeground`, `XFillRectangle`, `XDrawString`
- XShmPutImage consistently returns 0 (failure) — fixed by passing `&shm_info` to `XShmCreateImage`
- Debug shows: shm_image=256x224, bpp=32, bpl=1024, shm_avail=1, shmid valid
- Simplified approach: keep initial 640x480 shm_image, render game at top-left corner
- F1/ESC handling: `process_events()` returns true on second press; menu checks `get_menu_button_pressed()` to distinguish F1/ESC (close menu) from SDL_QUIT (quit)
- Build files modified: `src/CMakeLists.txt`, `src/drivers/sdl2/CMakeLists.txt`, `src/drivers/sdl2/sdl2_impl.cpp`

## Relevant Files
- `docs/superpowers/specs/2026-05-31-x11-shm-video-design.md`: Design spec for X11 Shm backend
- `src/drivers/sdl2/x11_shm_video.h`: New header with x11_shm_video class declaration
- `src/drivers/sdl2/x11_shm_video.cpp`: New X11 Shm implementation (~300 lines)
- `src/drivers/sdl2/sdl2_impl.cpp`: Modified for conditional X11 vs OpenGL backend selection
- `src/CMakeLists.txt`: Added SDLRETRO_X11_SHM option and X11 dependency detection
- `src/drivers/sdl2/CMakeLists.txt`: Added x11_shm_video.cpp/h to sources
- `src/drivers/sdl1/sdl1_impl.cpp`: Added Alt+Enter fullscreen, F3 FPS toggle, F1 menu
- `src/drivers/sdl1/sdl1_video.cpp`: Implemented window_resized, FPS counter, pixel font only, fixed get_text_width_and_height
- `src/drivers/sdl1/sdl1_video.h`: Added fps_enabled, fps_enabled getter/setter
- `src/drivers/sdl1/sdl1_input.cpp`: Keymap with default population for unmapped keys
- `src/drivers/common/input_base.cpp`: on_km_input maps SDL keys to libretro button states
- `src/drivers/common/include/driver_base.h`: Added get_menu_button_pressed() virtual method
- `src/drivers/sdl1/include/sdl1_impl.h`: Override get_menu_button_pressed()
- `src/drivers/sdl2/include/sdl2_impl.h`: Override get_menu_button_pressed()
- `src/gui/menu_base.cpp`: event_loop sets input mode to mode_menu on open, fixed F1/ESC handling
- `src/gui/sdl_menu.cpp`: draw_text uses pixel font, highlight box positioning
- `src/drivers/sdl2/sdl2_video.cpp`: Existing OpenGL backend (reference)
