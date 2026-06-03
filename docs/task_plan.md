# Task Plan: Linux Framebuffer (/dev/fb0) Renderer

## Goal
Add a Linux framebuffer renderer that outputs directly to `/dev/fb0`, enabling sdlretro to run on embedded Linux devices without X11 or SDL (e.g., Raspberry Pi, retro handhelds with fbdev support).

## Current State
- sdlretro has two video backends: SDL1 (software, 16-bit SDL surfaces) and SDL2 (OpenGL/GLES textures)
- Both backends rely on SDL for window management, input, and event handling
- X11 Shared Memory backend exists as an optional SDL2 add-on
- Pixel format conversion (XRGB8888 to RGB565) is implemented in SDL1 video.cpp
- Bitmap font rendering (bmfont.inl) is used for menus without FreeType
- Config system supports resolution, scale, fullscreen, frame_limit, etc.
- Throttle system provides microsecond-precision frame timing

## Design Decision
**Approach: New fbdev driver with software rendering**
- New `fbdev` driver independent of SDL (frontend option 3)
- `fbdev_video` implements `video_base` with direct `/dev/fb0` mmap
- `fbdev_input` uses Linux evdev (`/dev/input/event*`) for keyboard/joystick
- Reuse bitmap font system from sdl1 (include bmfont.inl)
- Reuse throttle/config from driver_base (shared by all drivers)
- Software rendering with pixel format conversion (same as SDL1)

## Phases

### Phase 1: Create fbdev driver skeleton
- [x] Create `src/drivers/fbdev/` directory structure
- [x] Create `fbdev_impl.h` - fbdev_impl class inherits driver_base
- [x] Create `fbdev_impl.cpp` - opens `/dev/fb0`, creates fbdev_video and fbdev_input
- [x] Create `fbdev_video.h` - fbdev_video class inherits video_base
- [x] Create `fbdev_video.cpp` - implements video_base with /dev/fb0 mmap
- [x] Create `fbdev_input.h` - fbdev_input class inherits input_base
- [x] Create `fbdev_input.cpp` - reads from `/dev/input/event*` via evdev
- [x] Create `CMakeLists.txt` for fbdev driver library

### Phase 2: Video backend implementation
- [ ] Implement `window_resized()` - reopen `/dev/fb0` at new resolution
- [ ] Implement `game_resolution_changed()` - detect fbdev pixel format, set up buffers
- [ ] Implement `render()` - pixel format conversion + scaling
  - XRGB8888 (32-bit) -> RGB565 (16-bit) conversion
  - RGB565 passthrough (memcpy)
  - RGB1555 conversion
  - Scaling via h_line buffer (same as SDL1)
- [ ] Implement `frame_render()` - no-op (fbdev is direct scanout)
- [ ] Implement `frame_drawn()` - return draw state
- [ ] Implement bitmap font rendering (`draw_text`, `fill_rectangle`, etc.)
- [ ] Implement `get_framebuffer()` for cores that provide framebuffer

### Phase 3: Input backend implementation
- [ ] Open `/dev/input/event*` devices (enumerate available devices)
- [ ] Handle `struct input_event` for keys and joystick
- [ ] Map keyboard keys to game controls (same keymap as SDL1)
- [ ] Map joystick buttons/axes to gamepad
- [ ] Handle key repeat and state tracking

### Phase 4: CMake integration
- [ ] Add `FBDEV` option to `src/CMakeLists.txt`
- [ ] Add `SDLRETRO_FRONTEND == 3` for fbdev
- [ ] Add `fbdev` subdirectory to `src/drivers/CMakeLists.txt`
- [ ] Add conditional include in `main.cpp`
- [ ] Add `SDLRETRO_FBDEV` compile definition for platform detection

### Phase 5: Platform detection and fallback
- [ ] Detect Linux platform at compile time (`__linux__`)
- [ ] Add fallback message if `/dev/fb0` unavailable
- [ ] Handle permission issues (user needs access to /dev/fb0)
- [ ] Consider adding `FBDEV_DEVICE` config option for non-default fb devices

### Phase 6: Testing and refinement
- [ ] Test with RGB565 fbdev (most common on embedded)
- [ ] Test with XRGB8888 fbdev (Raspberry Pi)
- [ ] Test pixel format conversion accuracy
- [ ] Test scaling modes (integer scale, screen center)
- [ ] Test input (keyboard + joystick)
- [ ] Test menu overlay rendering
- [ ] Test frame timing (throttle with frame_limit)
- [ ] Test resolution changes during gameplay

## Files to Create
1. `src/drivers/fbdev/include/fbdev_impl.h`
2. `src/drivers/fbdev/fbdev_impl.cpp`
3. `src/drivers/fbdev/include/fbdev_video.h`
4. `src/drivers/fbdev/fbdev_video.cpp`
5. `src/drivers/fbdev/include/fbdev_input.h`
6. `src/drivers/fbdev/fbdev_input.cpp`
7. `src/drivers/fbdev/CMakeLists.txt`

## Files to Modify
1. `src/CMakeLists.txt` - Add FBDEV frontend option
2. `src/drivers/CMakeLists.txt` - Add fbdev subdirectory
3. `src/main.cpp` - Add fbdev driver selection
4. `src/drivers/sdl1/bmfont.inl` - Copy to fbdev (or share via common)

## Dependencies
- Linux kernel: `#include <linux/fb.h>`, `<sys/mman.h>`, `<fcntl.h>`, `<unistd.h>`
- evdev: `#include <linux/input.h>`, `<sys/stat.h>`, `<dirent.h>`
- No external libraries needed (pure Linux syscalls)

## Risks and Mitigations
| Risk | Mitigation |
|------|------------|
| `/dev/fb0` not available | Graceful error message, fallback to SDL |
| Permission denied | Document need for user to be in `video` group |
| Wrong pixel format on fbdev | Auto-detect from `fb_var_screeninfo` |
| No vsync tearing | Document vsync limitations, suggest `FBIO_WAITFORVSYNC` ioctl |
| Input device enumeration fails | Fallback to hardcoded `/dev/input/event0` or config option |
| No keyboard on headless system | Document need for keyboard/joystick input |

## Errors Encountered
| Error | Attempt | Resolution |
|-------|---------|------------|
| - | - | - |
