# Task Plan: Improved ZIP ROM Loading

## Goal
Improve ZIP ROM loading in main.cpp with better file selection and streaming extraction.

## Current State
- `main.cpp:155-182` — miniz extracts first non-text file from ZIP
- Loads entire ZIP into memory
- Skips `.txt` files, picks first other file
- No intelligent file selection

## Design (Approved)

### 1. Better File Selection
- Scan all ZIP entries, rank by priority
- Priority 1: `.bin`, `.img`, `.iso`, `.rom`, `.nes`, `.sfc`, `.gba`, `.md`, `.sg`, `.z64`, `.n64`, `.gg`, `.sms`, `.nds`, `.nds.gz`
- Priority 2: Any non-text file
- Skip: `.txt`, `.nfo`, `.info`, `.pdf`, `.html`, `.htm`, `.xml`, `.json`, `.cfg`, `.ini`

### 2. Streaming Extraction
- Use miniz streaming API
- Check file size before extracting (skip >256MB)
- Don't load entire ZIP into memory upfront

### 3. Implementation Location
- Keep in `main.cpp`, replace existing `155-182` block
- Extract helper functions: `find_best_rom_entry()`, `extract_zip_entry()`

### 4. Constraints
- No new dependencies (still uses miniz)
- fbdev driver unchanged
- Backward compatible

## Phases

### Phase 1: File Selection Helper (Low Risk) - DONE
- [x] Create `find_best_rom_entry()` function
- [x] Define ROM extension priority list
- [x] Define skip extension list
- [x] Scan ZIP entries and return best match

### Phase 2: Streaming Extraction (Low Risk) - DONE
- [x] Create `extract_zip_entry()` function
- [x] Use miniz streaming extraction (`mz_zip_reader_extract_to_heap`)
- [x] Add file size check (>256MB skip)
- [x] Replace inline ZIP code in main.cpp

### Phase 3: Integration & Testing
- [x] Wire up new functions in main.cpp
- [ ] Test with single-file ZIP
- [ ] Test with multi-file ZIP
- [ ] Test with large ZIP (>256MB should skip)

## Decisions
- Keep in main.cpp (user preference)
- ZIP only (no 7z/RAR/TAR.GZ)
- Pick first non-text file with better filtering
- Streaming extraction for memory efficiency

## Errors Encountered
| Error | Attempt | Resolution |
|-------|---------|------------|
| Segfault when loading ZIP ROM | — | `extract_zip_entry()` resizes `unzipped_data` then fails, leaving zero-filled data; now check return value before calling `load_game_from_mem()` |

## Phase Status
- [x] Phase 1: File Selection Helper
- [x] Phase 2: Streaming Extraction
- [ ] Phase 3: Integration & Testing (code wired, needs testing)
