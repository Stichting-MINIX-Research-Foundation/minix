# sys/stand/efiboot/bootx64 — x86_64 EFI bootloader build fixes

Documents the changes made to build `bootx64.efi` (the UEFI x86_64 bootloader)
with the MINIX cross-toolchain (Clang 3.6, target `x86_64-elf64-minix`) on the
`dev-efi3` branch.

## Background

`sys/stand/efiboot/bootx64/` is the x86_64 variant of the MINIX/NetBSD EFI
bootloader. It existed in the tree but had never been built with the MINIX
cross-toolchain. The bootloader produces `bootx64.efi`, a PE32+ UEFI application.

## Build overview

The build uses `nbmake-amd64` and the following toolchain:

| Tool | Path |
|------|------|
| Compiler | `x86_64-elf64-minix-clang` (Clang 3.6) |
| Linker | `x86_64-elf64-minix-ld` (GNU ld 2.34) |
| objcopy | `/usr/bin/objcopy` (host — see below) |

The output pipeline: arch source files → object files → `bootx64.efi.so` (ELF
shared object via `elf_x86_64_minix_efi.lds`) → `bootx64.efi` (PE32+ via
`objcopy`).

## Problems and fixes

### 1. Wrong `KLINK_MACHINE` and `LIBKERN_ARCH`

**File:** `sys/stand/efiboot/bootx64/Makefile`

The original values were `x86-64` (a non-existent arch name). This caused
`bsd.klinks.mk` to create a `machine` symlink pointing to
`sys/arch/x86-64/include` (doesn't exist), so `#include <machine/cdefs.h>`
failed immediately.

```makefile
# Before
KLINK_MACHINE=  x86-64
LIBKERN_ARCH=   x86-64

# After
KLINK_MACHINE=  amd64      # sys/arch/amd64/include exists
LIBKERN_ARCH=   x86_64     # sys/lib/libkern/arch/x86_64/ exists
```

`bsd.klinks.mk` with `KLINK_MACHINE=amd64` creates:
- `machine → sys/arch/amd64/include`
- `amd64 → sys/arch/amd64/include`
- `i386 → sys/arch/i386/include` (amd64 also gets this)
- `x86 → sys/arch/x86/include`
- `x86_64 → sys/arch/x86_64/include` (via `MACHINE_CPU=x86_64`)

### 2. Unknown Clang warning flag

**File:** `sys/stand/efiboot/bootx64/Makefile`

`-Wno-error=address-of-packed-member` was added in Clang 4.0; Clang 3.6 rejects
it as an error (with `-Werror`). Changed to `-Wno-unknown-warning-option` so
that any unrecognised warning flag is silently suppressed.

### 3. `efibootx64.c` used wrong include path

**File:** `sys/stand/efiboot/bootx64/efibootx64.c`

`#include "efiboot.h"` failed because `efiboot.h` lives in the parent
`efiboot/` directory and the `-I` path for `${.CURDIR}/../` is not on the
compiler's search path. Fixed to `#include "../efiboot.h"`, matching the
pattern used in `bootaa64/efibootaa64.c`.

### 4. Missing arch-specific function implementations

**Files:** `bootx64/efiboot_machdep.h`, `bootx64/efibootx64.c`

`exec.c` calls `efi_dcache_flush()` and `efi_boot_kernel()`, while
`exec_multiboot2.c` calls `multiboot2()`.  None of these were declared or
implemented for x86_64.

The `bootia32/efiboot_machdep.h` is the reference pattern. Updated
`bootx64/efiboot_machdep.h` to declare:

```c
typedef unsigned long physaddr_t;
void multiboot2(physaddr_t, physaddr_t, uint32_t);
int exec_multiboot2(const char *, const char *);
void *probe_multiboot2(const char *);
void efi_dcache_flush(u_long, u_long);
void efi_boot_kernel(u_long[]);
void efi_md_init(void);
void efi_md_show(void);
```

Implementations added to `efibootx64.c`:

- **`efi_dcache_flush`** — no-op; x86_64 has cache-coherent DMA.
- **`efi_boot_kernel`** — calls the `startprog64` trampoline (which transitions
  from 64-bit EFI mode to 32-bit protected mode before jumping to the NetBSD
  kernel entry).
- **`efi_md_show`** — empty; no CPU info to display at boot prompt.
- **`multiboot2`** — calls through the `multiboot64` function pointer (the
  trampoline allocated by `efi_md_init` from the `multiboot64.S` code).

The old `startprog()` and `multiboot()` functions that referenced undefined
variables (`efi_kernel_start`, `efi_loadaddr`, `efi_kernel_size`) from an
earlier API were removed.

### 5. `DEV_BSIZE` and related constants undefined for x86_64

**File:** `sys/arch/amd64/include/param.h`

Under `#ifdef __x86_64__` (i.e., 64-bit native mode), `DEV_BSIZE`, `DEV_BSHIFT`,
`BLKDEV_IOSIZE`, and `MAXPHYS` were not defined and `i386/param.h` was not
included.  These are needed by `libsa` (`stand.h`, `disklabel.c`, etc.).

Added inside the `#ifdef __x86_64__` block:

```c
#define DEV_BSHIFT      9
#define DEV_BSIZE       (1 << DEV_BSHIFT)
#define BLKDEV_IOSIZE   2048
#ifndef MAXPHYS
#define MAXPHYS         (64 * 1024)
#endif
```

### 6. `<elf.h>` not available under `-nostdinc`

**File:** `sys/external/bsd/gnu-efi/dist/gnuefi/reloc_x86_64.c`

The EFI standalone build uses `-nostdinc` so system headers like `<elf.h>`
(normally in `/usr/include`) are not reachable. All needed types (`Elf64_Dyn`,
`Elf64_Rel`) and macros (`ELF64_R_TYPE`, `DT_NULL`, etc.) are available via
`<sys/exec_elf.h>`.  The x86_64-specific relocation constants (`R_X86_64_NONE`,
`R_X86_64_RELATIVE`) live in `<machine/elf_machdep.h>` (resolved via the
`machine →` symlink created by `bsd.klinks.mk`).

Also added a forward declaration for `_relocate` to satisfy `-Wmissing-prototypes`
(following the pattern in `reloc_aarch64.c`).

```c
/* Before */
#include <elf.h>

/* After */
#if defined(__NetBSD__) || defined(__minix__)
#include <sys/types.h>
#include <sys/exec_elf.h>
#include <machine/elf_machdep.h>
#else
#include <elf.h>
#endif

EFI_STATUS _relocate(long, Elf64_Dyn *, EFI_HANDLE, EFI_SYSTEM_TABLE *);
```

### 7. Missing MINIX-specific EFI linker script for x86_64

**New file:** `sys/external/bsd/gnu-efi/dist/gnuefi/elf_x86_64_minix_efi.lds`

The `x86_64-elf64-minix-ld` linker only understands `elf64-x86-64-minix`
output format; it does not know the generic `elf64-x86-64` format used by the
standard `elf_x86_64_efi.lds`.  Created a MINIX-specific copy with:

```
OUTPUT_FORMAT("elf64-x86-64-minix", "elf64-x86-64-minix", "elf64-x86-64-minix")
```

`Makefile.efiboot`'s linker script selection was extended to cover `x86_64`
alongside `ia32`:

```makefile
.if defined(__MINIX) && (${GNUEFIARCH} == "ia32" || ${GNUEFIARCH} == "x86_64")
LDSCRIPT?= ${EFIDIR}/gnuefi/elf_${GNUEFIARCH}_minix_efi.lds
```

### 8. MINIX cross-objcopy lacks PE/COFF support

**File:** `sys/stand/efiboot/bootx64/Makefile`

`x86_64-elf64-minix-objcopy` was built without PE/COFF (`pei-x86-64`) target
support (the MINIX `config.bfd` entry only includes ELF targets). The host
system's `objcopy` (binutils on the build machine) supports both
`elf64-x86-64-minix` input and `pei-x86-64` output.

Overridden in `bootx64/Makefile` after all BSD `.mk` includes:

```makefile
# MINIX cross-objcopy lacks PE/COFF support; override after bsd includes
.if defined(__MINIX)
OBJCOPY= objcopy
.endif
```

The override must come after `.include "${.CURDIR}/../Makefile.efiboot"` because
`bsd.init.mk` (included from `Makefile.efiboot`) unconditionally sets
`OBJCOPY= ${TOOLDIR}/bin/...`.

### 9. PHDR linker warning / "not enough room for program headers" *(fixed)*

**File:** `sys/external/bsd/gnu-efi/dist/gnuefi/elf_x86_64_minix_efi.lds`

`x86_64-elf64-minix-ld` emitted `PHDR segment not covered by LOAD segment`
because with `ImageBase = 0` the ELF auto-generated PT_PHDR is not covered by
any explicit PT_LOAD.

The fix adds a `PHDRS` block with explicit segment names (suppressing the
auto-generated PT_PHDR) and assigns each SECTIONS entry to a named segment:

```
PHDRS
{
  text PT_LOAD;
  data PT_LOAD;
  dyn  PT_DYNAMIC;
}
```

`FILEHDR PHDRS` must **not** be added to the text entry: with `ImageBase = 0`
the linker would try to fit the ELF header and program-header table before
`.hash` at offset 0, producing a fatal `not enough room for program headers`
error.  Without `FILEHDR`, no PT_PHDR is generated and the link succeeds cleanly.

Section assignments added: `:text` on `.hash`, `.gnu.hash`, `.eh_frame`,
`.text`, `.reloc`; `:data` on `.data`, `.note.gnu.build-id`, `.rela`,
`.dynsym`, `.dynstr`; `:data :dyn` on `.dynamic`.

### 10. Cross-objcopy PE support *(config.bfd updated; toolchain rebuild needed)*

**File:** `external/gpl3/binutils/dist/bfd/config.bfd`

The `x86_64-*-minix*` entry in `config.bfd` was extended to include PE/COFF
targets:

```
targ_selvecs="i386_elf32_minix_vec i386_elf32_vec iamcu_elf32_vec x86_64_pei_vec i386_pei_vec"
```

This change takes effect only after rebuilding the cross-toolchain.  Until
then, the host-`objcopy` override in `bootx64/Makefile` remains necessary:

```makefile
# MINIX cross-objcopy lacks PE/COFF support; override after bsd includes
.if defined(__MINIX)
OBJCOPY= objcopy
.endif
```

## Files changed

| File | Change |
|------|--------|
| `sys/stand/efiboot/bootx64/Makefile` | Fix `KLINK_MACHINE`, `LIBKERN_ARCH`, warning flag; add `OBJCOPY` override |
| `sys/stand/efiboot/bootx64/efiboot_machdep.h` | Add `physaddr_t`, function declarations |
| `sys/stand/efiboot/bootx64/efibootx64.c` | Fix include path; add `efi_dcache_flush`, `efi_boot_kernel`, `efi_md_show`, `multiboot2` |
| `sys/arch/amd64/include/param.h` | Add `DEV_BSHIFT`, `DEV_BSIZE`, `BLKDEV_IOSIZE`, `MAXPHYS` for x86_64 |
| `sys/external/bsd/gnu-efi/dist/gnuefi/reloc_x86_64.c` | Replace `<elf.h>` with portable headers; add `_relocate` prototype |
| `sys/external/bsd/gnu-efi/dist/gnuefi/elf_x86_64_minix_efi.lds` | New: MINIX-specific EFI linker script; PHDRS block added |
| `sys/stand/efiboot/Makefile.efiboot` | Extend MINIX linker script selection to x86_64 |
| `external/gpl3/binutils/dist/bfd/config.bfd` | Add `x86_64_pei_vec i386_pei_vec` to x86_64-minix targ_selvecs |

## Runtime bug fixes *(code review)*

Five correctness bugs found by code review and fixed in `exec_multiboot2.c`,
`exec.c`, and `efiblock.c`.

### 1. Wrong kernel entry point for EFI64 boot *(fixed)*

**File:** `sys/stand/efiboot/exec_multiboot2.c` — `start_multiboot2()`

`start_multiboot2()` selected the kernel entry address by checking only
`mpp_entry` (Multiboot2 header tag type 3, the `MULTIBOOT_HEADER_TAG_ENTRY_ADDRESS`
non-EFI entry).  The MINIX x86_64 kernel provides its UEFI entry via tag type 9
(`MULTIBOOT_HEADER_TAG_ENTRY_ADDRESS_EFI64`) pointing to `multiboot_entry64_efi`,
which expects to be entered in 64-bit long mode.  Tag type 3 points to
`multiboot_entry32`, which is 32-bit protected-mode code.  The bootloader
(`multiboot64.S`) jumps to the resolved address while still in long mode, so
entering `multiboot_entry32` (32-bit code) in that state causes an immediate fault.

The `mpp_entry_elf64` field was already parsed and stored at tag-scan time (line
1398); it just was not consulted during entry selection.

**Fix:** After the type-3 check, added a `#ifdef __LP64__` block that overwrites
`entry` with `mpp_entry_elf64->entry_addr` when that tag is present:

```c
if (mpp->mpp_entry)
    entry = mpp->mpp_entry->entry_addr;
#ifdef __LP64__
/* Prefer the EFI64-specific entry point (tag type 9) on 64-bit builds;
 * it expects to be entered in long mode, unlike the type-3 entry which
 * is 32-bit protected-mode code. */
if (mpp->mpp_entry_elf64)
    entry = mpp->mpp_entry_elf64->entry_addr;
#endif
```

### 2. GOP NULL dereference on headless systems *(fixed)*

**File:** `sys/stand/efiboot/exec_multiboot2.c` — `start_multiboot2()`

`efi_gop_found()` returns NULL when no GOP framebuffer is present (headless
server, serial console).  Immediately after the call, `gop->Mode->Mode` was
dereferenced with no NULL check, crashing before the kernel was started.
Every other GOP call site in the file correctly guards with `if (gop == NULL)`.

**Fix:** Wrapped the `gop->Mode->Mode` read and `efi_gop_setmode()` call:

```c
if (gop != NULL) {
    mode = gop->Mode->Mode;
    efi_gop_setmode(mode);
}
```

### 3. `mbi_cmdline` buffer too small — truncated kernel command line *(fixed)*

**File:** `sys/stand/efiboot/exec_multiboot2.c` — `mbi_cmdline()`

`cmdlen` was computed as `strlen(mbp->mbp_args) + 1`, counting only the args
string.  The `snprintf` call used format `"%s %s"` to write
`mbp_file + " " + mbp_args`, which requires
`strlen(mbp_file) + 1 + strlen(mbp_args) + 1` bytes.  Both the heap allocation
(`sizeof(*mbt) + cmdlen`) and the `snprintf` limit (`cmdlen`) were too small,
silently truncating the file path.  The kernel received a garbled or empty
cmdline, causing `root=` and other early-boot options to be lost.

**Fix:**

```c
/* strlen(file) + ' ' + strlen(args) + '\0' */
cmdlen = strlen(mbp->mbp_file) + 1 + strlen(mbp->mbp_args) + 1;
```

### 4. `FreePages` called with byte count instead of page count *(fixed)*

**File:** `sys/stand/efiboot/exec.c` — `efi_efirng_alloc()`

On EFI RNG failure the cleanup path called:

```c
uefi_call_wrapper(BS->FreePages, 2, addr, size);
```

`FreePages` takes a page count, not a byte count.  `size` is `EFI_PAGE_SIZE`
(4096 bytes), so this attempted to free 4096 pages (~16 MB) starting at `addr`,
corrupting the UEFI memory map.  The adjacent `AllocatePages` call already used
`EFI_SIZE_TO_PAGES(size)` correctly.

**Fix:**

```c
uefi_call_wrapper(BS->FreePages, 2, addr, EFI_SIZE_TO_PAGES(size));
```

### 5. `efiblock` IoAlign allocation undersized — heap overflow on aligned I/O *(fixed)*

**File:** `sys/stand/efiboot/efiblock.c` — `efi_block_do_read_blockio()`

When `IoAlign > 1`, `alloc_size` was computed as
`roundup(blkbuf_size, IoAlign)` — rounding the transfer size up to the next
alignment boundary.  However, `blkbuf_start = roundup2(blkbuf, IoAlign)` can
advance the start pointer by up to `IoAlign - 1` bytes past `blkbuf`.
`ReadBlocks` then writes `blkbuf_size` bytes starting at `blkbuf_start`, which
may extend up to `IoAlign - 1` bytes past the end of the pool allocation,
corrupting adjacent heap blocks.

Example: `blkbuf_size = 512`, `IoAlign = 512`, `AllocatePool` returns an
address ending in `0x01`.  `alloc_size = 512` (already a multiple), but
`blkbuf_start` is `511` bytes into the allocation.  `ReadBlocks` writes 512
bytes starting 511 bytes in — 511 bytes past the end of the 512-byte pool.

**Fix:** Allocate `blkbuf_size + IoAlign - 1` bytes so that after the alignment
shift there is always room for the full transfer:

```c
if (bdev->bio->Media->IoAlign > 1)
    alloc_size = blkbuf_size + bdev->bio->Media->IoAlign - 1;
```

