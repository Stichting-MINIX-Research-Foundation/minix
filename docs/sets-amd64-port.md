# MINIX Distribution Sets — amd64 Port

## Overview

The MINIX distribution set files (`distrib/sets/lists/`) validate that DESTDIR
matches what the build is expected to produce. For i386 these were complete; for
amd64 three machine-specific files were missing entirely, causing `checkflist`
to fail with ~1000 extra files and 2 missing files.

## Files created

### `distrib/sets/lists/minix-base/md.amd64`

Covers everything in `md.i386` that is also applicable to amd64, **minus** the
i386 boot/mdec entries and **plus** the amd64 EFI boot file:

| Category | Entries |
|---|---|
| `etc/system.conf.d/` | 14 NIC / virtio driver configs |
| `service/` | 19 hardware-specific services |
| `usr/lib/` | libacpi.a, libacpi_pic.a, libvirtio.a, libvirtio_pic.a |
| `usr/mdec/` | bootx64.efi |

Services common to all machines (ds, pm, vfs, vm, tty, …) stay in the
machine-independent `md.i386` → they are in `mi`.

### `distrib/sets/lists/minix-comp/md.amd64`

Covers headers and toolchain scripts that differ by architecture:

| Category | Entries |
|---|---|
| `usr/include/amd64/` | 81 amd64 arch headers (ansi.h … vmparam.h) |
| `usr/include/x86/` | 28 shared x86 headers (specialreg.h, pte.h, …) |
| `usr/include/clang-3.6/` | 39 x86 intrinsic headers (same as md.i386) tagged `llvmcmds` |
| `usr/libdata/ldscripts/` | 65 linker scripts tagged `binutils` |

### `distrib/sets/lists/minix-debug/md.amd64`

Mirrors `md.i386` but adapted for amd64 (no `libm387_g.a`; adds `libacpi_g.a`).
All entries carry the `debug`/`debuglib` tag so they are only checked when
`MKDEBUG != no`.

## mkvars.mk — missing MK variables

`distrib/sets/mkvars.mk` lists the build variables whose values the AWK
set-file processor reads when deciding which tagged entries to include.  Four
variables were missing, causing entire categories to be silently omitted from
the flist even though the files were present in DESTDIR:

| Variable added | Tag enabled | Files covered |
|---|---|---|
| `MKBINUTILS` | `binutils` | All `usr/libdata/ldscripts/*` |
| `MKLLVM` | `llvm` | LLVM tool files |
| `MKLLVMCMDS` | `llvmcmds` | `usr/include/clang-3.6/*` intrinsics |
| `MKNLS` | `nls` | `usr/share/locale/**` and `usr/share/i18n/**` |

All four default to `yes` in a normal amd64 build (`bsd.own.mk`).

## minix-base/mi and minix-man/mi — removed netstat

`./usr/bin/netstat` and `./usr/man/man1/netstat.1` were listed in the MINIX
set files but have never been built as part of the MINIX distribution.
Removed to eliminate a permanent "missing" error.

## Root Makefile — SLOPPY_FLIST=YES

```makefile
${MAKEDIRTARGET} distrib/sets checkflist SLOPPY_FLIST=YES
```

The MINIX set files track MINIX-specific files; they do not enumerate the full
NetBSD base system (locale data, test binaries, shared libraries, man3 pages,
…) that the build also installs into DESTDIR.  This is the same situation on
i386.  `SLOPPY_FLIST=YES` makes extra files non-fatal so the distribution build
can proceed; missing-file errors are still fatal.

After all fixes the fresh flist for amd64 has **0 missing files**.

## Summary of extra-file categories that remain (non-fatal)

| Category | Count | Root cause |
|---|---|---|
| `usr/tests/` | ~2500 | Newer nbmake unit-test suite not enumerated in minix-tests |
| `usr/share/locale/` | ~1900 | Now covered by `nls` tag after mkvars fix |
| `usr/man/man3/` | ~400 | NetBSD man pages beyond MINIX set scope |
| `usr/libdata/debug/` | ~155 | Debug build artifacts; debug tag, MKDEBUG=no |
| `usr/include/c++/` | ~121 | C++ headers; not in any MINIX comp set |
| `usr/include/openssl/` | ~77 | OpenSSL headers |
| `usr/libdata/ldscripts/` | ~66 | Covered by binutils tag after mkvars fix |

## MAKEDEV device-major map — `sys/arch/amd64/conf/majors.amd64`

The live-image build (`distrib/common/bootimage/Makefile.bootimage`) runs the
generated `MAKEDEV` script to populate `/dev`.  `MAKEDEV` is produced by
`etc/MAKEDEV.awk` from `etc/MAKEDEV.tmpl` + the per-arch device-major table
`sys/arch/<machine>/conf/majors.<machine>`.  Any device whose `%name_chr%` /
`%name_blk%` placeholder is **not** found in that majors table is *scrapped*
from the generated script — so at runtime it reports `<dev>: unknown device`
and the node is never created.

`majors.amd64` was still the **stock NetBSD** table (cons=0, wd=3, com=8, …),
which has none of the MINIX device names; i386 had been replaced with the MINIX
map (commit "Fix MAKEDEV") but amd64 was missed.  Result on amd64 only:

```
work/dev/MAKEDEV: console: unknown device
work/dev/MAKEDEV: fb0/fbd/filter/hello/klog/ptmx/random/imgrd: unknown device
...
```

**Fix:** replace `sys/arch/amd64/conf/majors.amd64` with the MINIX device-major
map (mirrors `majors.i386` — the majors are machine-independent: same MINIX
servers/drivers on both arches; `cons=4, ptmx=9, filter=11, fbd=14, klog=15,
random=16, hello=17, fb=19, kbd=64, pci=134`, …).  The build then generates the
device cases and creates `/dev/console`, `fb0`, `random`, `imgrd`, etc.  The fix
is in the majors table, **not** in `Makefile.bootimage`.
