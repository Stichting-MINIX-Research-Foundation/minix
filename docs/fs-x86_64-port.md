# x86_64 Filesystem Server Porting

## Overview

The MINIX filesystem servers in `minix/fs/` required minimal changes for x86_64
because nearly all of their code is architecture-independent.  The only server
with arch-specific code is `procfs`, which reads CPU topology and IPC vector
addresses from the kernel.

## Scope

| Server | Status | Notes |
|--------|--------|-------|
| `mfs`    | Ready | No arch-specific code |
| `pfs`    | Ready | No arch-specific code |
| `ext2`   | Ready | `u32_t` usage is for on-disk format fields (correct by ext2 spec) |
| `isofs`  | Ready | No arch-specific code |
| `ptyfs`  | Ready | No arch-specific code |
| `procfs` | Ported | See below |
| `hgfs`   | i386-only | `libhgfs/backdoor.S` is 32-bit-only x86 assembly; excluded via `minix/fs/Makefile` |
| `vbfs`   | i386-only | Depends on `libhgfs`/`libvboxfs`; same exclusion |

## Build System

`minix/fs/Makefile` already correctly excludes `hgfs` and `vbfs` on non-i386:

```makefile
.if ${MACHINE_ARCH} == "i386"
SUBDIR+=  hgfs
SUBDIR+=  vbfs
.endif
```

All other servers build unconditionally on all architectures.

## procfs Changes

### cpuinfo.c

The archconst.h include was gated on `__i386__` only.  Extended to cover
`__x86_64__` so that `CPU_VENDOR_INTEL`, `CPU_VENDOR_AMD`, and `CPU_VENDOR_UNKNOWN`
are defined when building for amd64:

```c
#if defined(__i386__)
#include "../../kernel/arch/i386/include/archconst.h"
#elif defined(__x86_64__)
#include "../../kernel/arch/x86_64/include/archconst.h"
#endif
```

All guards around the `x86_flag[]` array, `print_x86_cpu_flags()`, and the
`print_cpu()` vendor/family/model/stepping block were changed from
`#if defined(__i386__)` to `#if defined(__i386__) || defined(__x86_64__)`.

`struct cpu_info` is identical between i386 and x86_64 (same fields: `vendor`,
`family`, `model`, `stepping` as `u8_t`; `freq` and `flags[2]` as `u32_t`).
The kernel fills it via the same CPUID leaf 0/1 path on both architectures.

### root.c

Four `#if defined(__i386__)` guards were extended to
`#if defined(__i386__) || defined(__x86_64__)`:

- `<machine/pci.h>` include
- `root_pci()` forward declaration
- `{ "pci", ... }` entry in `root_files[]`
- `{ "cpuinfo", ... }` entry in `root_files[]`
- `root_pci()` definition and closing comment

The `PRINT_ENTRYPOINT` macro used `%08lx` for IPC vector addresses.  On x86_64
these are 64-bit kernel addresses; `%08lx` would truncate the zero-padding.
Changed to emit `%016lx` on x86_64:

```c
#if defined(__x86_64__)
#define PRINT_ENTRYPOINT(name) \
    buf_printf("%016lx T %s(k)\n", \
        (unsigned long)_minix_ipcvecs.name, #name)
#else
#define PRINT_ENTRYPOINT(name) \
    buf_printf("%08lx T %s(k)\n", \
        (unsigned long)_minix_ipcvecs.name, #name)
#endif
```

### pid.c

`pid_map()` printed virtual memory region addresses with `%08lx`.  On x86_64,
`vri_addr` and `vri_length` are `vir_bytes` = `unsigned long` (64-bit).
Changed `/proc/<pid>/map` output to use `%016lx` on x86_64:

```c
#if defined(__x86_64__)
    buf_printf("%016lx-%016lx %c%c%c\n",
#else
    buf_printf("%08lx-%08lx %c%c%c\n",
#endif
        vri[i].vri_addr,
        vri[i].vri_addr + vri[i].vri_length, ...);
```

## Files Changed

| File | Change |
|------|--------|
| `minix/fs/procfs/cpuinfo.c` | `#elif __x86_64__` archconst include; extend x86 flag/vendor guards |
| `minix/fs/procfs/root.c` | Extend four i386-only guards; `%016lx` in `PRINT_ENTRYPOINT` for x86_64 |
| `minix/fs/procfs/pid.c` | `%016lx` in `pid_map` for x86_64 `vir_bytes` addresses |

No changes were required to `minix/fs/Makefile` or any non-procfs server.
