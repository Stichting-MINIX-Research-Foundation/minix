# MINIX Servers — x86_64 Port

This document records the changes made to port `minix/servers/`, `minix/fs/procfs/`, and the kernel signal path to x86_64. The VM server was already done; this covers everything else.

## Files changed

| File | Change |
|------|--------|
| `sys/arch/amd64/include/frame.h` | Added `struct sigframe_sigcontext` for MINIX signal frames |
| `minix/kernel/system/do_sigsend.c` | x86_64 register save + AMD64 ABI arg passing + frame alignment |
| `minix/kernel/system/do_sigreturn.c` | x86_64 register restore + rflags filter + FPU restore |
| `minix/servers/pm/misc.c` | `"amd64"` / `"x86_64"` in `utsname` and `uts_tbl[]` |
| `minix/servers/mib/hw.c` | `mach="amd64"` / `arch="x86_64"` strings |
| `minix/servers/mib/mib.h` | x86_64 archconst.h include |
| `minix/servers/is/dmp_kernel.c` | `proctab_dmp()` implementation for x86_64 |
| `minix/servers/vfs/coredump.c` | Forward decls changed from `Elf32_*` to `Elf_*` |
| `minix/fs/procfs/cpuinfo.c` | Archconst include + CPU-info printing extended to x86_64 |
| `minix/fs/procfs/root.c` | `/proc/pci` and `/proc/cpuinfo` entries enabled for x86_64 |

Servers with no arch-specific code (no changes needed): `ds`, `rs`, `sched`, `vfs` (exec path), `ipc`, `input`, `devman`. MFS and ext2 are also clean.

---

## Signal delivery on x86_64

The MINIX signal path is split across three layers. All arch-specific work is in the kernel; PM and the libc trampoline are already portable.

### sigframe_sigcontext layout (`sys/arch/amd64/include/frame.h`)

```
frp+0:   sf_ra_sigreturn   (uint64_t)  — return address to __sigreturn trampoline
frp+8:   sf_scp            (pointer)   — &frp->sf_sc; at (%rsp) after handler ret
frp+16:  sf_code + pad     (int, int)  — FPE sub-code from fpu_sigcontext()
frp+24:  sf_sc             (struct sigcontext) — saved register context
```

On handler entry the kernel arranges:
- `rsp = frp`  (satisfying AMD64 ABI: `rsp % 16 == 8`)
- `rdi = signo`, `rsi = sf_code`, `rdx = sf_scp`  (ABI argument registers)
- `rip = sm_sighandler`

After handler `ret`:
- `rip = sf_ra_sigreturn` (__sigreturn trampoline)
- `rsp = frp + 8`
- trampoline: `movq (%rsp), %rdi; jmp sigreturn`  → scp in rdi, calls `sigreturn(scp)`

### do_sigsend (`minix/kernel/system/do_sigsend.c`)

The x86_64 block (guarded `#elif defined(__x86_64__)`) saves all 16 GPRs plus rip/rflags/rsp into `fr.sf_sc`, saves the FPU state if `MF_FPU_INITIALIZED` is set, then aligns `frp` so that `(reg_t)frp & 0xf == 8`. After the common `data_copy_vmcheck`, it sets:

```c
rp->p_reg.di = signo;
rp->p_reg.si = fr.sf_code;   /* FPE sub-code */
rp->p_reg.dx = fr.sf_scp;    /* pointer to saved context */
rp->p_misc_flags |= MF_CONTEXT_SET;
```

The `sf_ra_sigreturn` field is set to `smsg.sm_sigreturn` (the trampoline address registered by libc via `sigaction`).

### do_sigreturn (`minix/kernel/system/do_sigreturn.c`)

Restores all 16 GPRs + rip/rflags/rsp from the `sigcontext` copied in from user space. `rflags` is filtered through `X86_FLAGS_USER` before restore (same as i386). FPU restore is now guarded `#if defined(__i386__) || defined(__x86_64__)`.

### Stackframe field mapping

```
struct stackframe_s   struct sigcontext (x86_64)
────────────────────  ───────────────────────────
p_reg.di          →   sc_rdi
p_reg.si          →   sc_rsi
p_reg.fp          →   sc_rbp
p_reg.bx          →   sc_rbx
p_reg.dx          →   sc_rdx
p_reg.cx          →   sc_rcx
p_reg.retreg      →   sc_rax
p_reg.r8-r15      →   sc_r8 - sc_r15
p_reg.pc          →   sc_rip
p_reg.psw         →   sc_rflags
p_reg.sp          →   sc_rsp
```

---

## pm/misc.c — uname strings

MINIX reports:

```
uname -m  →  "amd64"    (uts_val.machine, matches NetBSD amd64 convention)
uname -p  →  "x86_64"   (uts_tbl[0], the ISA name)
```

---

## mib server

`mib/hw.c` exposes `HW_MACHINE = "amd64"` and `HW_MACHINE_ARCH = "x86_64"` via `sysctl hw.machine` / `hw.machine_arch`.

`mib/mib.h` includes `kernel/arch/x86_64/include/archconst.h` for `CONFIG_MAX_CPUS` (used in `HW_NCPU`). The x86_64 archconst defines `TSS_INDEX(cpu) = TSS_INDEX_FIRST + cpu * 2` (two GDT slots per 64-bit TSS, vs one slot on i386).

---

## vfs/coredump.c — ELF type fix

The forward declarations at the top of the file used hardcoded `Elf32_*` types while the function bodies used the arch-neutral `Elf_*` aliases (which expand to `Elf64_*` on amd64). All forward declarations are now `Elf_*`.

---

## procfs

`/proc/cpuinfo` and `/proc/pci` are now enabled for x86_64 by extending the `#if defined(__i386__)` guards to `#if defined(__i386__) || defined(__x86_64__)` in both `cpuinfo.c` and `root.c`. The CPU info code uses the same `struct cpu_info` fields (vendor, family, model, stepping, freq, flags) and the same `x86_flag[]` table as i386 — x86_64 CPUs report the same CPUID feature bits.
