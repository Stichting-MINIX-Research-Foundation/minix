# libminixfs x86_64 Port

## Overview

`minix/lib/libminixfs` (buffer cache + block I/O helpers for file system
servers) compiled cleanly on i386 but produced format-string and sign-compare
errors under the x86_64-elf64-minix-clang toolchain.  The root cause is that
several primitive types widen on LP64:

| Type | i386 | x86_64 |
|------|------|--------|
| `size_t` / `uint64_t` / `dev_t` | 32 / 64 / 32 bit | 64 / 64 / 64 bit |
| `off_t` | `long long` (64-bit) | `long` (64-bit) |
| `%llu` / `%llx` format | correct for `uint64_t` | **wrong** — `uint64_t` is `unsigned long`, needs `%lu` / `%lx` |

## Files Changed

### `minix/lib/libminixfs/bio.c`

**Line 135 — signed/unsigned comparison in range check**

```c
/* before */
if (pos < 0 || bytes > SSIZE_MAX || pos > INT64_MAX - bytes + 1)

/* after */
if (pos < 0 || bytes > SSIZE_MAX || pos > (off_t)(INT64_MAX - bytes + 1))
```

On x86_64 both `off_t` and `size_t` are 64-bit, but `off_t` is signed `long`
while `size_t` is unsigned `long`.  The expression `INT64_MAX - bytes + 1`
promotes to `unsigned long` (because `INT64_MAX` is `long long` and `bytes` is
`unsigned long`; the subtraction converts to `unsigned long long` then truncates
to `unsigned long` on the final `+1`).  Comparing signed `pos` against an
unsigned value triggers `-Wsign-compare`.  The `(off_t)` cast is safe because
the preceding `bytes > SSIZE_MAX` guard guarantees `bytes ≥ 1` and
`bytes ≤ INT64_MAX`, so the result fits in `off_t`.

### `minix/lib/libminixfs/cache.c`

**Line 338 — `%llu` for `u64_t`**

```c
/* before */
printf("cache: unaligned lmfs_get_block_ino ino_off %llu\n", ino_off);

/* after */
printf("cache: unaligned lmfs_get_block_ino ino_off %"PRIu64"\n", ino_off);
```

`u64_t` is `unsigned long long` on i386 but `unsigned long` on x86_64.
`PRIu64` is already used elsewhere in the file (e.g. block-number prints) and
resolves correctly on both architectures.

**Line 582 — `%llx` for `dev_t` and `uint64_t`**

```c
/* before */
panic("libminixfs: setblock of %p dev 0x%llx off "
    "0x%llx failed\n", bp->data, dev, dev_off);

/* after */
panic("libminixfs: setblock of %p dev 0x%"PRIx64" off "
    "0x%"PRIx64" failed\n", bp->data, (uint64_t)dev, dev_off);
```

`dev_t` on x86_64 is `unsigned long`; the `(uint64_t)` cast plus `PRIx64`
keeps the print correct on both 32-bit and 64-bit builds.

## No Makefile Changes Required

The library has no architecture guard in its Makefile and no arch-specific
source files, so it builds for x86_64 with the same `Makefile` used for i386.
