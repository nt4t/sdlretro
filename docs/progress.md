# Progress: Linux Framebuffer Renderer

## Session Log

### Session 1 - Planning
- [x] Explored codebase architecture for fbdev integration
- [x] Researched Linux framebuffer API requirements
- [x] Researched evdev input API requirements
- [x] Created task_plan.md with 6 phases
- [x] Created findings.md with architecture discoveries
- [x] Identified reusable components from SDL1 and driver_base
- [x] Documented risks and mitigations

### Session 2 - Implementation (Current)
- [x] Phase 1: Created fbdev driver skeleton
  - [x] Created `src/drivers/fbdev/` directory structure
  - [x] Created `fbdev_impl.h` - driver class
  - [x] Created `fbdev_impl.cpp` - opens /dev/fb0, mmap, creates backends
  - [x] Created `fbdev_video.h` - video backend header
  - [x] Created `fbdev_video.cpp` - video backend with pixel conversion, scaling, bitmap fonts
  - [x] Created `fbdev_input.h` - input backend header
  - [x] Created `fbdev_input.cpp` - evdev input with Linux keycode to SDLK mapping
  - [x] Created `CMakeLists.txt` for fbdev driver
  - [x] Copied bmfont.inl from sdl1
- [x] Phase 4 (partial): CMake integration
  - [x] Updated `src/CMakeLists.txt` - added FBDEV frontend option
  - [x] Updated `src/drivers/CMakeLists.txt` - added fbdev subdirectory
  - [x] Updated `src/main.cpp` - added fbdev driver selection

## Next Steps
- [ ] Phase 2: Test and refine video backend (render, scaling, pixel conversion)
- [ ] Phase 3: Test evdev input backend
- [ ] Phase 5: Platform detection and fallback
- [ ] Phase 6: Testing and refinement

## Errors Encountered
| Error | Attempt | Resolution |
|-------|---------|------------|
| SDL dependencies in fbdev_input.cpp | 1 | Replaced SDLK_* with Linux keycodes, added keycode_to_sdlk() mapping |
| SDL_Event in fbdev_impl.cpp | 1 | Removed SDL event loop, using evdev only |

## Decisions Made
| Decision | Rationale |
|----------|-----------|
| New fbdev driver (frontend option 3) | Independent of SDL, suitable for headless embedded |
| software rendering | Matches SDL1 pattern, no GPU dependencies |
| evdev for input | Standard Linux input, no SDL dependency |
| Reuse bmfont.inl from sdl1 | Avoid duplicating font data |
| Reuse throttle/config from driver_base | Shared by all drivers, already tested |
| Linux keycodes mapped to SDLK values | Reuses existing km_to_game_mapping logic |
