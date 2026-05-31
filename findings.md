# X11 Shm Video Backend - Findings

## Project Context
- sdlretro is a libretro frontend with layered architecture
- `video_base` is the abstract video interface
- SDL2 backend uses OpenGL (sdl2_video.cpp)
- SDL1 backend uses software rendering (sdl1_video.cpp)
- No existing X11-specific code in the project

## X11 Shm Requirements
- X11 development headers: `X11/Xlib.h`, `X11/extensions/XShm.h`
- XShm library: `-lXext` (usually bundled with X11)
- CMake: `find_package(X11 REQUIRED COMPONENTS Xext)`
- Pixel format: XRGB8888 (4 bytes per pixel, big-endian byte order)

## XShm Key API
- `XShmQueryExtension(Display*)` — check if XShm is available
- `XShmCreateImage(Display*, Visual*, depth, format, data, shmseg, bytesPerLine)` — create Shm image
- `XShmPutImage(Display*, Drawable, GC, XImage*, src_x, src_y, dst_x, dst_y, width, height, copy_flag)` — push frame
- `shmget()`, `shmat()`, `shmdt()`, `shmctl()` — shared memory management

## X11 Drawing Primitives for Overlay
- `XSetForeground(Display*, GC*, Pixel)` — set color
- `XFillRectangle(Display*, Drawable*, GC*, x, y, width, height)` — filled rect
- `XDrawString(Display*, Drawable*, GC*, x, y*, char*, length)` — text
- `XFreeGC()` — cleanup

## Existing Patterns to Follow
- `sdl2_video.cpp` — reference for `video_base` implementation
- `src/drivers/sdl2/CMakeLists.txt` — reference for adding source files
- `src/CMakeLists.txt` — reference for build options
