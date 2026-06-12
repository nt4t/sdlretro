# Building mGBA Libretro Core

## Prerequisites

- GCC compiler
- Make

## Build Steps

```bash
cd ~/src/mgba

# Apply compatibility fixes for modern glibc
sed -i '1a #include <string.h>' src/core/core-serialize.c
sed -i 's/-Wall -std=c99/-Wall -std=gnu99 -Wno-incompatible-pointer-types -Wno-error -D_GNU_SOURCE/' libretro-build/Makefile.rules
sed -i '18s/^/\/\/ /' include/mgba-util/formatting.h
sed -i '24s/^/\/\/ /' include/mgba-util/formatting.h

# Build
make -f Makefile.libretro platform=linux_x86_64
```

## Output

The built shared library will be at:

```
mgba_libretro.linux_x86_64.so
```

## Notes

- The mGBA source requires patches to compile with modern glibc (2.35+) due to `locale_t` and `strtof_l` type conflicts
- `strdup` requires `<string.h>` include
- `-Wno-incompatible-pointer-types` suppresses type mismatch warnings
- `-D_GNU_SOURCE` enables POSIX features
