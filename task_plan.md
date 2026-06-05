# Task Plan: Speed Up FBDEV Render

## Goal
Optimize fbdev framebuffer rendering performance for smoother gameplay.

## Current Bottlenecks
1. **Per-pixel 32->16 conversion** - `convert_xrgb8888_to_rgb565` loops pixel by pixel
2. **Per-pixel scaling** - `render_scaled` expands each pixel individually in inner loops
3. **No SIMD/vectorization** - pure scalar C++ loops
4. **LOG(INFO) inside hot path** - `convert_xrgb8888_to_rgb565` has a LOG call for 0xBDBEBD
5. **Redundant pointer arithmetic** - `dest += output_pitch` every row

## Optimization Phases

### Phase 1: Quick Wins (High Impact, Low Risk)
- [ ] Remove LOG(INFO) from `convert_xrgb8888_to_rgb565` hot path
- [ ] Use `memcpy` for 32->32 copy (already done), 16->16 copy (already done)
- [ ] Pre-compute offset calculations outside row loops
- [ ] Use `const uint32_t*` / `uint32_t*` instead of `const uint8_t*` casts

### Phase 2: SIMD Conversion (High Impact, Medium Risk)
- [ ] Implement NEON SIMD for 32->16 conversion (ARM platforms)
- [ ] Implement SSE2 for 32->16 conversion (x86 platforms)
- [ ] Fallback to scalar when SIMD unavailable
- [ ] Process 8-16 pixels per iteration

### Phase 3: Scaling Optimizations (Medium Impact)
- [ ] Use `memset` or optimized memcpy for horizontal expansion
- [ ] Replace per-pixel inner loop with word-sized fills
- [ ] Pre-compute scaled row buffers once, reuse every frame

### Phase 4: Memory Layout (Medium Impact)
- [ ] Eliminate intermediate h_line buffers for 1:1 rendering
- [ ] Use double-buffering to avoid framebuffer flicker
- [ ] Align allocations to cache line boundaries (64 bytes)

### Phase 5: Advanced (Low Impact, High Risk)
- [ ] MMAP with write-combining / uncached memory
- [ ] Use /dev/fb0 mmap with O_SYNC disabled if possible
- [ ] Framebuffer compression (if hardware supports)
- [ ] Dirty rectangle tracking (only redraw changed areas)

## Decisions
- Target: ARM embedded (NEON) and x86 (SSE2)
- Keep scalar fallback for compatibility
- No external dependencies (no NEON intrinsics from third-party libs)

## Errors Encountered
| Error | Attempt | Resolution |
|-------|---------|------------|
| (none yet) | | |
