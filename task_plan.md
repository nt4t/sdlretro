# Task Plan: Optional Frame Limit for SDL1 Render

## Goal
Add an optional frame limit configuration option that allows users to cap the frame rate for SDL1 render (e.g., limit to 30fps for games that run at 120fps due to missing VBL delay).

## Current State
- Frame timing is handled by `throttle` class in `throttle.cpp`
- Throttle is initialized with core's reported FPS in `driver_base::post_load()`
- Main loop in `driver_base::run()` calls `check_wait()` after each frame
- No frame limit config option exists currently
- SDL1 and SDL2 share the same driver_base, throttle, and config system

## Design Decision
**Approach: Config-driven throttle override in driver_base**
- Add `frame_limit` config field (0 = disabled, use core FPS; >0 = limit to this FPS)
- Override throttle in `post_load()` and when config changes
- Works for both SDL1 and SDL2 (shared driver_base)
- Simple, minimal changes, no core timing modifications

## Phases

### Phase 1: Add config field
- [ ] Add `frame_limit` field to `cfg` class in `cfg.h`
- [ ] Add getter/setter methods
- [ ] Add load/save support in `cfg.cpp`
- Default: 0 (disabled)

### Phase 2: Apply frame limit in driver_base
- [ ] Modify `post_load()` to use frame_limit if set
- [ ] Apply override after core's `GET_SYSTEM_AV_INFO`
- [ ] Handle config changes (menu re-entry)

### Phase 3: Test
- [ ] Test with core that runs too fast (needs frame limit)
- [ ] Test with core that runs at correct speed (frame_limit=0)
- [ ] Test changing frame_limit at runtime

## Files to Modify
1. `src/libretro/include/cfg.h` - Add frame_limit field
2. `src/libretro/cfg.cpp` - Add load/save for frame_limit
3. `src/drivers/common/driver_base.cpp` - Apply frame limit to throttle

## Errors Encountered
| Error | Attempt | Resolution |
|-------|---------|------------|
| - | - | - |
