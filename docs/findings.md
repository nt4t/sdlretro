# Findings: Linux Framebuffer Renderer

## Architecture Discoveries

### video_base Interface
- Pure virtual methods (5): `window_resized`, `game_resolution_changed`, `render`, `frame_render`, `frame_drawn`
- Non-pure methods have empty defaults: `draw_text`, `fill_rectangle`, `clear`, `flip`, etc.
- `get_framebuffer()` returns nullptr by default, can be overridden for cores that provide framebuffer

### Driver Creation Pattern
- Driver class inherits `driver_base`, implements `init()`, `deinit()`, `unload()`
- Factory template in `driver_base.h`: `create_driver<T>()`
- Driver creates video, audio, input backends via `std::make_shared`
- `driver_base::run()` handles main loop, throttle, menu handling

### Pixel Format Conversion (from SDL1)
- XRGB8888 -> RGB565: extract 5/6/5 bits, pack into 16-bit
- RGB565 -> RGB565: direct memcpy
- Scaling uses h_line buffer for horizontal + vertical expansion
- BPP detection: `pixel_format == 1 ? 32 : 16`

### Throttle System
- Microsecond precision timing via `throttle::check_wait()`
- Initialized with core FPS in `driver_base::post_load()`
- Can be overridden by `frame_limit` config
- Auto-frameskip when frame is late (`set_skip_frame()`)

### Bitmap Font System
- 1-bit per pixel glyph data in `bmfont.inl`
- Two font sets: small (8x7) for GCW, big (9x5) for normal displays
- Glyphs can be cached as 32-bit pixels or rendered directly
- `CODE_WITH_TYPE` macro handles 16/32-bit framebuffer rendering

### CMake Frontend Selection
- `SDLRETRO_FRONTEND` cache variable: "SDL1", "SDL2", or to be added "FBDEV"
- Defines `SDLRETRO_FRONTEND=1/2/3` preprocessor macro
- Conditional compilation gates driver selection in `main.cpp`
- Drivers are statically linked libraries

## Linux Framebuffer Requirements

### Required Headers
```cpp
#include <linux/fb.h>      // struct fb_var_screeninfo, FBIOGET_FSCREENINFO
#include <sys/mman.h>      // mmap, munmap
#include <fcntl.h>         // open, O_RDWR
#include <unistd.h>        // close
#include <sys/ioctl.h>     // ioctl
```

### fbdev Workflow
1. `open("/dev/fb0", O_RDWR)`
2. `ioctl(fd, FBIOGET_FSCREENINFO, &finfo)` - get fixed info (stride, line_length)
3. `ioctl(fd, FBIOGET_VSCREENINFO, &vinfo)` - get variable info (xres, yres, bpp)
4. `mmap(NULL, finfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)`
5. Write pixels directly to `fb_ptr`
6. No flip needed (fbdev IS the scanout buffer)

### evdev Workflow (Input)
1. Enumerate `/dev/input/event*` files
2. `open()` each device, `ioctl(fd, EVIOCGNAME(), ...)` to identify
3. `read(fd, &event, sizeof(struct input_event))` in poll loop
4. `event->type == EV_KEY` for keyboard/joystick buttons
5. `event->type == EV_ABS` for joystick axes
6. `event->type == EV_SYN` for frame sync

### Pixel Formats on fbdev
- Most embedded devices: RGB565 (16-bit), little-endian
- Raspberry Pi: XRGB8888 (32-bit)
- Detection: `vinfo.bits_per_pixel` and `vinfo.red/green/blue.offset`

## Reusable Components

### From SDL1
- Pixel format conversion (XRGB8888 -> RGB565)
- Scaling algorithm (h_line buffer)
- Bitmap font rendering
- Keymap (keyboard to game controls)

### From driver_base
- Throttle system (frame timing)
- Config system (resolution, scale, fullscreen, frame_limit)
- Main loop (run(), process_events(), menu handling)
- Message queue (on-screen notifications)

### From cfg
- Resolution settings
- Scale factor
- Fullscreen toggle
- Frame limit override

## Platform Constraints
- Linux-only (`__linux__` detection)
- Requires root or `video` group membership for `/dev/fb0` access
- No window manager, direct scanout
- No multi-tasking UI (terminal may interfere)
- Vsync optional via `ioctl(FBIO_WAITFORVSYNC)`
