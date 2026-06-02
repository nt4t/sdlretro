# Findings: Optional Frame Limit for SDL1 Render

## Render Pipeline Flow
1. `driver_base::run()` main loop calls `core->retro_run()` then `video->frame_render()`
2. Frame timing handled by `throttle` class (microsecond precision)
3. Throttle initialized with core's FPS from `RETRO_ENVIRONMENT_GET_SYSTEM_AV_INFO`
4. After each frame: `check_wait()` returns microseconds to sleep (or negative if late)
5. If `usecs > 0`: `usleep(usecs)` sleeps; if `usecs <= 0`: frameskip enabled

## Throttle Mechanism
- `throttle::reset(double fps)` - sets `frame_time = 1000000/fps` microseconds
- `throttle::check_wait()` - returns remaining sleep time, advances `next_frame`
- `throttle::skip_check()` - advances `next_frame` without sleeping (no frame drawn)
- Time source: `QueryPerformanceCounter` (Windows) or `clock_gettime` (POSIX)

## Config System
- Global singleton `g_cfg` in `cfg.cpp`
- Config stored as JSON in `{store_dir}/cfg/sdlretro.json`
- Fields defined in `cfg.h` with getters/setters
- No frame_limit field exists currently

## Key Integration Points
- `driver_base::post_load()` (line ~718): throttle initialized with core FPS
- `driver_base::run()` (line ~87): main loop with throttle check
- `driver_base::run()` (line ~82): menu re-entry, could re-apply config

## Where Frame Limit Can Be Applied
**Best location: `post_load()` after core provides FPS**
- Override throttle FPS if `frame_limit > 0`
- Simple one-line change: `frame_throttle->reset(effective_fps)`
- Works for both SDL1 and SDL2 (shared driver_base)

## Alternative Locations (not recommended)
- `throttle::check_wait()` - would require adding limit parameter
- Main loop after `check_wait()` - would add extra sleep on top of throttle
- Per-driver (sdl1_video/sdl2_video) - duplicates logic, doesn't shared code

## Config Field Design
```cpp
double frame_limit_fps = 0;  // 0 = disabled, >0 = limit to this FPS
```
- 0 means "use core FPS" (current behavior)
- Positive value overrides core FPS
- Can be changed at runtime via config file or menu (future)
