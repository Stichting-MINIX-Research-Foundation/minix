# x86_64 ucontext Support for libmthread

## Overview

This change adds x86_64 (AMD64) support for the MINIX `ucontext` family of
functions (`getcontext`, `setcontext`, `makecontext`, `swapcontext`), enabling
`libmthread` cooperative threading on 64-bit MINIX. Previously only i386 and
ARM were supported.

## Files Modified

| File | Change |
|------|--------|
| `sys/arch/amd64/include/mcontext.h` | Added `mc_magic`/`mc_flags` fields, `MCF_MAGIC`, `_MC_FPU_SAVED`, `_UC_MACHINE_STACK`/`SET_STACK` macros, and `setmcontext`/`getmcontext` declarations under `#ifdef __minix` |
| `minix/lib/libc/arch/x86_64/sys/ucontextoffsets.cf` | Rewrote for x86_64 register names; added R8, R9, R12–R15 |
| `minix/lib/libc/arch/x86_64/sys/ucontext.S` | Full implementation of `getcontext`/`setcontext`/`ctx_start` for AMD64 |
| `minix/lib/libc/arch/x86_64/sys/Makefile.inc` | Added `ucontext.S` to `SRCS`; restored genassym rule to build `ucontextoffsets.h` |
| `minix/lib/libc/sys/_ucontext.c` | Added `#elif defined(__x86_64__)` branch to `makecontext` |
| `minix/lib/libmthread/allocate.c` | Extended two `#if defined(__i386__) || defined(__arm__)` guards to also include `__x86_64__` |

## Design

### MINIX fast path

`mthread_getcontext` sets `_UC_IGNFPU | _UC_IGNSIGM` on every context it
creates.  When both flags are set, `getcontext` and `setcontext` skip the
PM/kernel round-trip (signal mask, FPU state) and operate entirely in user
space, saving only general-purpose registers.  This fast path is preserved
unchanged from the i386 implementation.

### Register save/restore strategy

Only callee-saved registers need to be captured by `getcontext` for the
cooperative fast path:

| Saved register | Purpose |
|----------------|---------|
| RBP, RBX | Standard callee-saved GP |
| R12–R15 | Standard callee-saved GP |
| RIP | Return address (read from `0(%rsp)` at call entry) |
| RSP | Caller's stack pointer (= `%rsp + 8` at call entry) |

`setcontext` restores those plus all six SysV argument registers
(RDI, RSI, RDX, RCX, R8, R9) to correctly serve contexts created by
`makecontext` with `argc > 0`.  RIP is stashed in the caller-saved scratch
register R11 before RDI is clobbered by its own restore, then jumped through
at the end.

### `makecontext` argument layout (SysV AMD64 ABI)

The first 6 arguments are stored in `gregs[0..5]`, which map directly to the
SysV AMD64 argument registers:

| `gregs` index | Register | `_REG_*` constant |
|---------------|----------|-------------------|
| 0 | RDI | `_REG_RDI` |
| 1 | RSI | `_REG_RSI` |
| 2 | RDX | `_REG_RDX` |
| 3 | RCX | `_REG_RCX` |
| 4 | R8  | `_REG_R8`  |
| 5 | R9  | `_REG_R9`  |

Arguments beyond 6 are pushed onto the context's stack.  RSP is aligned to
16 bytes before `ctx_start` runs, satisfying the ABI requirement that RSP is
16-byte aligned at the point of a `call` instruction (so the called function
sees RSP ≡ 8 mod 16 after the implicit push of the return address).

### `ctx_start` trampoline

`makecontext` sets RIP to `ctx_start` and stores the function pointer in R12
and the `ucontext_t *` in R13.  Both are callee-saved registers, so they
survive the `call *%r12` to the thread function.  When the thread function
returns, `ctx_start` moves R13 into RDI and calls `resumecontext(ucp)`, which
follows `uc_link` or calls `exit(0)`.

### `mc_magic` validity guard

`MCF_MAGIC` (`0xc0ffee`) is added to `mcontext_t` under `#ifdef __minix`
following the same pattern already used on i386.  `makecontext` requires the
field to be set before it will configure the context; `setcontext` validates it
before restoring.  `getcontext` writes the magic at the end of its fast path.

### Stack guard pages in libmthread

The guard page is placed at the low end of the `mmap`-returned region
(`[stackaddr, stackaddr + MTHREAD_GUARDSIZE)`, mapped `PROT_NONE`).  The
usable stack region starts immediately above the guard and grows downward, so
an overflow will fault on the guard page.  On teardown, `munmap` reconstructs
the original mapping boundaries by subtracting `MTHREAD_GUARDSIZE` from the
stored `ss_sp` and adding it back to `ss_size`.

## Suggested next steps

- **Build test**: `nbmake-amd64 obj && nbmake-amd64 dependall` in
  `minix/lib/libc` and `minix/lib/libmthread` to verify the genassym pipeline
  produces correct offsets and the assembly compiles cleanly.
- **Runtime test**: Boot a MINIX x86_64 image and run the
  `minix/tests/lib/libmthread/` suite.
- **`_UC_MACHINE_SET_PC` macro** (low priority): Add this to
  `sys/arch/amd64/include/mcontext.h` and use it in `_ucontext.c::makecontext`
  to match the `_UC_MACHINE_SET_*` style used by the i386 and ARM branches.
