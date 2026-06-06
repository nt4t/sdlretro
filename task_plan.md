# Task Plan: Speed Up FBDEV Render

## Goal
Optimize fbdev framebuffer rendering performance for smoother gameplay.

## Current Bottlenecks
1. **Per-pixel 32->16 conversion** - `convert_xrgb8888_to_rgb565` loops pixel by pixel (now SIMD-accelerated)
2. **Per-pixel scaling** - `render_scaled` expands each pixel individually in inner loops (now SIMD-accelerated)
3. **No SIMD/vectorization** - pure scalar C++ loops (now AArch64 NEON + SSE2)
4. **LOG(INFO) inside hot path** - `convert_xrgb8888_to_rgb565` has a LOG call for 0xBDBEBD (removed)
5. **Redundant pointer arithmetic** - `dest += output_pitch` every row (pre-computed)

## Optimization Phases

### Phase 1: Quick Wins (High Impact, Low Risk) - DONE
- [x] Remove LOG(INFO) from `convert_xrgb8888_to_rgb565` hot path
- [x] Use `memcpy` for 32->32 copy (already done), 16->16 copy (already done)
- [x] Pre-compute offset calculations outside row loops
- [x] Use `const uint32_t*` / `uint32_t*` instead of `const uint8_t*` casts

### Phase 2: SIMD Conversion (High Impact, Medium Risk) - DONE
- [x] Implement NEON SIMD for 32->16 conversion (ARM platforms, AArch64 only)
- [x] Implement SSE2 for 32->16 conversion (x86 platforms)
- [x] Fallback to scalar when SIMD unavailable
- [x] Process 8 pixels per iteration

### Phase 3: Scaling Optimizations (Medium Impact) - DONE
- [x] Use word-sized fills for horizontal expansion
- [x] Replace per-pixel inner loop with SIMD vector replication
- [x] Pre-compute scaled row buffers using h_line_16/h_line_32 members
- [x] AArch64 NEON: 4 pixels/cycle (32-bit), 8 pixels/cycle (16-bit via vdupq_n_u16)
- [x] SSE2: 4 pixels/cycle (32-bit), 8 pixels/cycle (16-bit)
- [x] ARMv7 NEON: removed (non-standard intrinsics on target toolchain), falls back to scalar
- [x] Scalar fallback for all platforms

### Phase 4: Memory Layout (Medium Impact) - DONE
- [x] Double-buffering to avoid framebuffer flicker (fb_back_buffer with aligned_alloc)
- [x] Align allocations to cache line boundaries (64 bytes) for h_line_16/h_line_32
- [x] render_1to1: use memcpy for 32->32 (was per-pixel loop)
- [x] render_scaled: fix 32->16 in-place expansion bug (expand to separate h_line_32 buffer)

### Phase 5: MMAP Optimizations (Medium Impact) - DONE
- [x] Write-combining mmap (MAP_WRITECOMbine with fallback to MAP_SHARED)
- [x] madvise(MADV_SEQUENTIAL) for sequential access hint
- [x] msync(MS_ASYNC) after double-buffering copy when WC enabled
- [x] IOC_FB_CLEAN_CACHE ioctl attempt

### Phase 6: Parallel Rendering (Medium Impact) - DONE
- [x] Split scaled render rows across multiple threads
- [x] Per-thread h_line buffers (heap-allocated)
- [x] Auto-detect thread count from hardware_concurrency()
- [x] Log parallel render status at startup

### Phase 7: Advanced (Low Impact, High Risk)
- [ ] Framebuffer compression (if hardware supports)
- [ ] Dirty rectangle tracking (only redraw changed areas)

## Decisions
- Target: ARM embedded (NEON AArch64) and x86 (SSE2)
- ARMv7 NEON: removed due to non-standard intrinsics on target toolchain (vld1_u32 returns uint32x2_t instead of uint32x4_t)
- Keep scalar fallback for compatibility
- No external dependencies (no NEON intrinsics from third-party libs)

## Errors Encountered
| Error | Attempt | Resolution |
|-------|---------|------------|
| ARMv7 NEON vld1_u32 returns uint32x2_t | Phase 2 ARMv7 NEON | Removed ARMv7 NEON, scalar fallback |
| ARMv7 NEON vshrq_n_u32 type mismatch | Phase 3 ARMv7 NEON expand | Removed ARMv7 NEON expand functions |

## Phase Status
- [x] Phase 1: Quick Wins
- [x] Phase 2: SIMD Conversion
- [x] Phase 3: Scaling Optimizations
- [x] Phase 4: Memory Layout
- [x] Phase 5: MMAP Optimizations
- [x] Phase 6: Parallel Rendering
- [ ] Phase 7: Advanced
