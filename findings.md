# Findings: FBDEV Render Optimization

## Current Render Paths

### render_1to1
- **32->32**: Already uses direct pixel copy (fast)
- **16->32**: Per-pixel RGB1555->XRGB8888 conversion (slow, no SIMD)
- **32->16**: Calls `convert_xrgb8888_to_rgb565` per row (SIMD-accelerated: AArch64 NEON 8px/cycle, SSE2 8px/cycle)
- **16->16**: Already uses `memcpy` (fast)

### render_scaled
- **32->32**: SIMD horizontal expansion (AArch64 NEON 4px/cycle, SSE2 4px/cycle)
- **16->32**: SIMD conversion + expansion (AArch64 NEON 8px/cycle bulk, SSE2 8px/cycle bulk)
- **32->16**: `convert_xrgb8888_to_rgb565` + SIMD horizontal expansion (AArch64 NEON 8px/cycle + 4px/cycle, SSE2 8px/cycle + 4px/cycle)
- **16->16**: SIMD horizontal expansion (AArch64 NEON 4px/cycle, SSE2 8px/cycle)

## Key Discovered Issues
1. `convert_xrgb8888_to_rgb565` had a LOG(INFO) call inside the loop for 0xBDBEBD - removed
2. All scaling paths used per-pixel inner loops instead of word-sized fills - replaced with SIMD
3. No SIMD acceleration on any path - now AArch64 NEON + SSE2
4. ARMv7 NEON intrinsics non-standard on target toolchain (vld1_u32 returns uint32x2_t, vshrq_n_u32 type mismatches) - falls back to scalar

## Platform Considerations
- AArch64: NEON intrinsics process 8x 32-bit or 8x 16-bit values in parallel (standard)
- x86: SSE2 processes 4x 32-bit or 8x 16-bit values in parallel
- ARMv7: NEON intrinsics non-standard on some toolchains, scalar fallback
- Detection: use `__ARM_NEON` + `__aarch64__` for AArch64, `__SSE2__` for x86

## Reference Implementations
- SDL2's pixel conversion uses SIMD when available
- Linux kernel framebuffer drivers use optimized copy routines
- Simple NEON 8->5 conversion: shift+mask with VSHRN
