## Goal
Speed up SDL1 software rendering backend to match or approach SDL2/OpenGL performance on resource-constrained devices (GCW-Zero, Raspberry Pi, etc.).

## Current State Analysis

### SDL1 Rendering Pipeline
1. `SDL_SetVideoMode` → `SDL_SWSURFACE` with 16-bit or 32-bit pixels
2. `SDL_LockSurface` → direct pixel buffer access
3. Software scaling via nested loops (scale × scale per pixel)
4. `SDL_Flip` → software surface blit to display
5. `SDL_UnlockSurface` → repeat

### Performance Bottlenecks
1. **Software scaling** (lines 137-166 sdl1_video.cpp): Nested loops scale each pixel individually — O(n²) per frame
2. **Pixel font rendering** (lines 334-382 sdl1_video.cpp): Bit-by-bit glyph rendering with per-pixel writes
3. **Surface lock/unlock** every frame adds overhead
4. **No integer scaling** — forces expensive floating-point math
5. **No hardware acceleration** — everything is CPU-bound
6. **`usleep(10000)`** in `game_resolution_changed` and `gui_popup` adds 10ms delays
7. **`SDL_Flip`** is slower than `SDL_UpdateWindowSurface` on modern systems

## Speedup Plan

### Phase 1: Quick Wins (No Architecture Changes)

#### 1.1 Remove usleep delays
- **Location**: `sdl1_video.cpp:69`, `sdl1_video.cpp:389`
- **Change**: Remove `usleep(10000)` calls
- **Impact**: Eliminates 20ms per menu open/close, reduces input lag

#### 1.2 Use SDL_UpdateWindowSurfaceRect instead of SDL_Flip
- **Location**: `sdl1_video.cpp:202-207`
- **Change**: Replace `SDL_Flip` with `SDL_UpdateWindowSurfaceRect` for partial updates
- **Impact**: Reduces unnecessary surface copies

#### 1.3 Optimize 1:1 scaling path
- **Location**: `sdl1_video.cpp:123-136`
- **Change**: Use `SDL_BlitSurface` or `SDL_DisplayYUVOverlay` for 1:1 scaling when possible
- **Impact**: Faster 1:1 scaling on hardware that supports it

### Phase 2: Scaling Optimizations

#### 2.1 Add integer scaling mode
- **Location**: `cfg.h`, `sdl1_video.cpp`
- **Change**: Add integer scaling option (1x, 2x, 3x, etc.) that avoids floating-point math
- **Implementation**: Use simple pixel duplication with `memcpy` for each scaled row/column
- **Impact**: 2-5x faster scaling for integer multiples

#### 2.2 Optimize scale factor rendering
- **Location**: `sdl1_video.cpp:137-166`
- **Change**: Replace nested loops with row-by-row `memcpy` for horizontal scaling, then vertical scaling
- **Implementation**:
  ```cpp
  // Step 1: Scale horizontally using memcpy for each row
  // Step 2: Scale vertically by duplicating rows
  ```
- **Impact**: 3-10x faster scaling depending on scale factor

#### 2.3 Add bilinear filtering option
- **Location**: `cfg.h`, `sdl1_video.cpp`
- **Change**: Add simple bilinear interpolation for smoother scaling
- **Implementation**: Precompute scale lookup tables, use integer math
- **Impact**: Better visual quality with minimal performance cost

### Phase 3: Font Rendering Optimizations

#### 3.1 Pre-render glyph cache
- **Location**: `sdl1_video.cpp:299-385`
- **Change**: Pre-render frequently used characters to offscreen surfaces
- **Implementation**: Cache first 128 ASCII characters, blit from cache instead of bit-by-bit rendering
- **Impact**: 10-50x faster text rendering for cached characters

#### 3.2 Use SDL_CreateRGBSurface for glyph surfaces
- **Location**: `sdl1_video.cpp`
- **Change**: Create pre-rendered glyph surfaces at startup
- **Implementation**: Render each glyph once, store as SDL_Surface, blit as needed
- **Impact**: Eliminates per-frame bit manipulation

### Phase 4: Architecture Improvements

#### 4.1 Add OpenGL fallback for SDL1
- **Location**: New file `sdl1_gl_video.cpp`
- **Change**: Detect OpenGL support at runtime, use hardware acceleration when available
- **Implementation**: Similar to SDL2 backend but with SDL1 window management
- **Impact**: Near-SDL2 performance on devices with OpenGL support

#### 4.2 Add YUV overlay rendering
- **Location**: `sdl1_video.cpp`
- **Change**: Use `SDL_DisplayYUVOverlay` for YUV content (if cores support it)
- **Implementation**: Check for YUV pixel format, use overlay when available
- **Impact**: Hardware-accelerated rendering for compatible cores

#### 4.3 Add triple buffering option
- **Location**: `sdl1_video.cpp:17-23`
- **Change**: Enable triple buffering to reduce input lag
- **Implementation**: Define `SDL_TRIPLEBUF` at compile time
- **Impact**: Smoother frame delivery, reduced stutter

### Phase 5: Advanced Optimizations

#### 5.1 Add SIMD acceleration
- **Location**: `sdl1_video.cpp`
- **Change**: Use SSE/NEON intrinsics for scaling operations
- **Implementation**: Detect CPU capabilities, use optimized paths when available
- **Impact**: 2-4x faster scaling on ARM/x86

#### 5.2 Add multi-threaded rendering
- **Location**: New thread in `sdl1_video.cpp`
- **Change**: Render next frame while displaying current frame
- **Implementation**: Double-buffer rendering, use condition variables for synchronization
- **Impact**: Hide rendering latency behind display

#### 5.3 Add frame skipping optimization
- **Location**: `sdl1_video.cpp:100-104`
- **Change**: Intelligent frame skipping based on system performance
- **Implementation**: Track frame time, skip frames when falling behind
- **Impact**: Maintain target FPS on underpowered hardware

## Implementation Priority

1. **Phase 1** (Quick Wins) — 1-2 hours, immediate impact
2. **Phase 2** (Scaling) — 4-6 hours, biggest performance gain
3. **Phase 3** (Font Rendering) — 2-3 hours, improves UI responsiveness
4. **Phase 4** (Architecture) — 1-2 weeks, major rework
5. **Phase 5** (Advanced) — 1-2 weeks, marginal gains

## Success Metrics
- 2x faster scaling at 2x resolution
- 5x faster text rendering
- <5ms frame delivery latency
- Match SDL2 performance on devices with OpenGL support

## Relevant Files
- `src/drivers/sdl1/sdl1_video.cpp`: Main render implementation
- `src/drivers/sdl1/sdl1_video.h`: Header file
- `src/drivers/sdl2/sdl2_video.cpp`: OpenGL backend (reference)
- `src/libretro/cfg.h`: Configuration options
- `src/libretro/include/helper.h`: Helper functions
