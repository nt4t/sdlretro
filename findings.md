# Findings: FBDEV Render Optimization

## Current Render Paths

### render_1to1
- **32->32**: Already uses direct pixel copy (fast)
- **16->32**: Per-pixel RGB1555->XRGB8888 conversion (slow)
- **32->16**: Calls `convert_xrgb8888_to_rgb565` per row (slow)
- **16->16**: Already uses `memcpy` (fast)

### render_scaled
- **32->32**: Per-pixel horizontal expansion, then memcpy per scaled row
- **16->32**: Per-pixel RGB1555->XRGB8888 + horizontal expansion (slow)
- **32->16**: Per-pixel 32->16 conversion + horizontal expansion (slowest)
- **16->16**: Per-pixel horizontal expansion (slow)

## Key Discovered Issues
1. `convert_xrgb8888_to_rgb565` has a LOG(INFO) call inside the loop for 0xBDBEBD - this is extremely expensive
2. All scaling paths use per-pixel inner loops instead of word-sized fills
3. No SIMD acceleration on any path
4. `render_scaled` 32->16 path calls `convert_xrgb8888_to_rgb565` with `pixels=1` repeatedly (worst case)

## Platform Considerations
- ARM: NEON intrinsics can process 8x 32-bit values in parallel
- x86: SSE2 can process 4x 32-bit values in parallel
- Detection: use `__ARM_NEON` and `__SSE2__` preprocessor defines

## Reference Implementations
- SDL2's pixel conversion uses SIMD when available
- Linux kernel framebuffer drivers use optimized copy routines
- Simple NEON 8->5 conversion: shift+mask with VSHRN
