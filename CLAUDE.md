# MINIX dev-efi3 — Claude notes

> Per-subsystem x86_64 port detail lives in `docs/*.md`. This file is the
> quick-reference: core rules + a pointer to the doc that has the full story.

## Build
- `nbmake-amd64` builds x86_64 MINIX; `nbmake-i386` for i386
- `nbmake-amd64` is NOT on PATH in non-interactive shells and `$TOOLDIR` is unset there; `export TOOLDIR=$(ls -d "$PWD"/../build/tooldir.Linux-*-x86_64)` then run `$TOOLDIR/bin/nbmake-amd64`
- Iterate one component: `$TOOLDIR/bin/nbmake-amd64 -C <dir>` builds, `... -C <dir> install` stages into `build/destdir.amd64` (binary lands in `destdir.amd64/service/<name>` for servers/drivers). Boot modules still need a live-image rebuild to take effect (see QEMU section); standalone `/service` drivers can be swapped into the image
- Architecture naming split: kernel/headers use `amd64` (sys/arch/amd64/), libc/minix use `x86_64` (minix/include/arch/x86_64/, minix/lib/libc/arch/x86_64/)

## Assembly (PIC)
- i386 .S: every `PIC_PLT()` call must be wrapped with `PIC_PROLOGUE` / `PIC_EPILOGUE`
- x86_64 .S: `PIC_PROLOGUE`/`PIC_EPILOGUE` are invalid (i386-only GOT setup); use `PIC_PLT(sym)` bare — no wrappers

## ucontext / libmthread
- `TOOL_GENASSYM` generates struct-offset headers from `.cf` files; i386 Makefile.inc is the reference pattern
- amd64 gregs[0..5] = RDI/RSI/RDX/RCX/R8/R9 (SysV AMD64 ABI order); `sys/arch/amd64/include/frame_regs.h`
- MINIX fast path: libmthread sets `_UC_IGNFPU | _UC_IGNSIGM` to skip the kernel round-trip (signal mask + FPU); `mc_magic = MCF_MAGIC (0xc0ffee)` is a MINIX-only validity guard in `mcontext_t` (checked by `setcontext`/`makecontext`)
- libmthread arch guards use `#if defined(__i386__) || defined(__arm__)` — add `|| defined(__x86_64__)`
- Details: `docs/libmthread-x86_64-context.md`

## Headers / destdir
- `minix/include/arch/x86_64/include/Makefile` must use `INCSDIR= /usr/include/amd64` (not i386); `machine` → `amd64` symlink in destdir
- Reinstall MINIX arch headers: `$TOOLDIR/bin/nbmake-amd64 -C minix/include/arch/x86_64/include includes`
- Reinstall NetBSD amd64 system headers: `$TOOLDIR/bin/nbmake-amd64 -C sys/arch/amd64/include includes`
- `minix/include/arch/x86_64/include/fpu.h` gets installed to `amd64/fpu.h`, overwriting `sys/arch/amd64/include/fpu.h`; keep MINIX additions in the sys/ file and reinstall after edits

## libminc
- `minix/lib/libminc/Makefile` adds `-D_LIBMINC` to all files; strtoul.c and strtol.c also get `-D_STANDALONE`
- Headers with MINIX-specific types must use `(defined(_LIBMINC) || !defined(_STANDALONE))` — not just `!defined(_STANDALONE)` — to match `signal.h`'s pattern and work under the libminc combination

## libc build / relink
- The installed `libc.a`/`libc.so` is built from **`lib/libc`** (it pulls `minix/lib/libc/sys` in via `.PATH`); `nbmake-amd64 -C minix/lib/libc` does NOT update it — build `-C lib/libc`
- Statically-linked servers/tools must be rebuilt to pick up a libc change; a live-image repackage alone won't relink them
- `lib/libc/stdlib/malloc.c` (phkmalloc) compiles in two configs: libc, and libminc with `-D_LIBSYS` (→ `MALLOC_NO_SYSCALLS`); edits must build in both — guard syscall/`write`-using code with `#ifndef MALLOC_NO_SYSCALLS`. VM links this malloc statically: never add `write(2)`/IPC to it (a write from inside VM mid-alloc deadlocks VFS)

## pthread on MINIX (OpenSolaris/CDDL ports)
MINIX has no `pthread.h`. Use mthread with the pthread-compat layer instead:
```c
#ifdef __minix
#define _MTHREADIFY_PTHREADS
#include <minix/mthread.h>
#ifndef PTHREAD_CREATE_DETACHED
#define PTHREAD_CREATE_DETACHED MTHREAD_CREATE_DETACHED
#endif
#else
#include <pthread.h>
#ifndef __NetBSD__
#include <pthread_np.h>   /* omit if not needed */
#endif
#endif /* !__minix */
```
- `_MTHREADIFY_PTHREADS` must be defined **before** `#include <minix/mthread.h>`; it activates `pthread_t`, `pthread_mutex_t`, `pthread_cond_t`, `pthread_rwlock_t`, and all `pthread_*` function aliases
- `PTHREAD_CREATE_DETACHED` is not in the compat block; add it explicitly when needed
- `pthread_mutex_isowned_np` (NetBSD NP extension) has no mthread equivalent; stub to `(1)` under `#ifdef __minix`
- Files already pulling in `thread.h` (which does the above) need only guard bare `#include <pthread.h>` with `#ifndef __minix`
- `tsd_get/set/create` (`pthread_getspecific/setspecific/key_create`) work as-is once `_MTHREADIFY_PTHREADS` is active

## sys/mbuf.h on MINIX
- `paddr_t` in `sys/mbuf.h` is `_KERNEL`-only on amd64 → "unknown type name 'paddr_t'" in userspace. Fix: wrap `#include <sys/mbuf.h>` in `#ifndef __minix` (the files don't use mbuf types directly). Details: `docs/userland-x86_64-port.md` §3

## paddr_t in wscons / userspace drivers
- On amd64 `machine/types.h` defines `paddr_t` only under `_KERNEL`/`_KMEMUSER`/`_KERNTYPES`/`_STANDALONE`; MINIX userspace drivers get nothing (i386 always gets it as `__uint64_t`)
- Fix pattern: `#include <machine/types.h>` (guarded `!defined(_I386_MACHTYPES_H_) && !defined(_X86_64_TYPES_H_)`), then amd64 userspace fallback `typedef unsigned long paddr_t` under `defined(__x86_64__) && !_KERNEL && !_KMEMUSER && !_KERNTYPES && !_STANDALONE`. Details: `docs/drivers-x86_64.md` (wsdisplayvar.h)

## ZFS / osnet
- `MKZFS` defaults `yes` for `MACHINE == "amd64"` in `share/mk/bsd.own.mk`; guard with `!defined(__MINIX)` — MINIX has no ZFS, and all `external/cddl/osnet/` builds are gated on `MKZFS != "no"`. Details: `docs/userland-x86_64-port.md` §1

## binutils x86_64 config.h
- All five `arch/x86_64/config.h` files (libbfd, libopcodes, common, gas, ld) ship `ENABLE_NLS 1`; set `/* #undef ENABLE_NLS */` — MINIX has no gettext/`dgettext`. Details: `docs/userland-x86_64-port.md` §5

## MINIX libc missing syscall stubs
- Missing NetBSD syscalls are listed in `minix/lib/libc/sys/MISSING_SYSCALLS`; add stubs to `minix/lib/libc/sys/` and wire into `Makefile.inc`
- `_lwp_setprivate` (TLS %fs base): no-op stub; full impl needs kernel FSGSBASE (`CR4.FSGSBASE=1` + `wrfsbase`). `__msync13`: no-op (MINIX mmap doesn't cache writes). Details: `docs/userland-x86_64-port.md` §7

## Makefiles — MINIX guards
- `__MINIX` is a **make variable** (`__MINIX= yes`) set in `share/mk/sys.mk`; `.if defined(__MINIX)` works in normal nbmake builds
- Exception: Makefiles processed during the **tools/build phase** (before `sys.mk`) do not have `__MINIX` defined — use unconditional `?=` defaults (e.g. `USE_FILEMON?= no`) there
- C preprocessor MINIX guards use lowercase: `#ifdef __minix` (not `__MINIX`)

## x86_64 kernel compilation
- Kernel at `0xFFFFFFFF80400000`; objects linked into it need `CFLAGS += -mcmodel=kernel -mno-red-zone` (`arch/x86_64/Makefile.inc`). Pre-built archives (`-lsys`/`-lexec`/`-lminc`, `-mcmodel=small`) overflow `R_X86_64_32` → shadow them with kernel-compiled objects in `OBJS.kernel`. After CFLAGS changes run `nbmake-amd64 -C minix/kernel cleandir`. Details: `docs/kernel-build-x86_64-fixes.md`
- `phys_bytes` is `unsigned long` (64-bit on amd64); `phys_clicks` is `unsigned int` (32-bit). Use `vir_bytes` for kernel VAs. A function returning a page number must return `phys_clicks`/`phys_bytes`, never `int` — an `int` `NO_MEM` sign-extends into the 64-bit caller and bypasses `== NO_MEM` (was the VM `findbit` boot-to-login bug)
- Kernel link needs `-Wl,-z,max-page-size=0x1000` or `ld`'s 2 MB default pads the first `PT_LOAD` to file offset `0x200000`, putting the multiboot2 header outside the EFI loader's 32 KB search window → "not a multiboot2 kernel". Details: `docs/kernel-build-x86_64-fixes.md`

## Kernel config macros
- Use `#if CONFIG_FOO` (not `#ifdef CONFIG_FOO`) when the macro may be defined as `0` — `#ifdef` is truthy even for `CONFIG_FOO=0` and causes calls to guarded-away functions

## Clang
- x86_64-*-minix triple must be added manually in `external/bsd/llvm/dist/clang/lib/Basic/Targets.cpp`

## IPC message structs (x86_64)
- `_ASSERT_MSG_SIZE` enforces 56 bytes only on i386 (no-op on x86_64); `_MSG_PAYLOAD_SIZE` = 88 on x86_64, `message` = 96 bytes
- Design cross-arch structs to be exactly 56 bytes on i386 (they then fit the 88-byte x86_64 payload automatically); watch `int`-then-`uint64_t` alignment padding (`uint64_t` has align 4 on i386, 8 on x86_64). Details: `docs/ipc-abi-x86_64.md`

## LP64: addresses through 32-bit message fields (x86_64) — SYSTEMIC
- A 64-bit VA with bit 31 set (user stack top is `0xF0000000`) stored into a 32-bit `int` field (`m*_i*`) and read back into a `vir_bytes` is **sign-extended into the kernel half** → rejected as a kernel address. Use a 64-bit (`m*_ll1`/`m*ull1`) or pointer (`m*_p*`) field for any message macro carrying an address; audit `m*_i*` macros in `com.h`
- Adding a field to a `mess_N`: consume `padding[]` so existing offsets don't move; keep i386 size 56. Fixed instances (`VPF_ADDR`, `SVMCTL_MRG_ADDR`, `VFS_PM_PS_STR`) + detail: `docs/kernel-arch-x86_64.md` (Theme 1)

## LP64: libblockdriver iovec — read iov_addr, never iov_grant
- `bdr_transfer` hands the driver an `iovec_t` whose `iov_addr` (`vir_bytes`) holds a local address when `endpt==SELF`, else a grant ID (`at_wini` is the reference). Casting to `iovec_s_t` and reading `iov_grant` works on i386 but on amd64 sign-extends a SELF address with bit 31 set → EFAULT in `sys_vumap`/VM (was THE ahci root-mount bug; surfaces on the SELF partition-table read at open). Details: `docs/drivers-x86_64.md` (AHCI)

## LP64: fixed-width hardware descriptors & size-bound buffers
- DMA/hardware descriptor address fields read by devices must stay **fixed 32-bit** (`u32_t`, never `phys_bytes` — 8 bytes on amd64 inflates the struct); guard/bail to PIO for buffers >4 GB (e.g. at_wini IDE PRD `prdte_base`). Driver-wide DMA 64-bit audit: `docs/drivers-x86_64.md`
- Buffers sized for a `message`/struct grow on amd64 (`message` = 96 B vs 64): VM `vfs.c STATELEN` must be `sizeof(message)` (FDLOOKUP stores a whole message); VM `slaballoc.c SLABSIZES` cap bumped 200→256 (amd64 `vfs_request_node` = 224 B)

## Kernel cross-address-space copy / demand paging (x86_64)
- `lin_lin_copy` (`arch/x86_64/memory.c`): `createpde()` returns **0** for a not-present process page; you MUST `return EFAULT_SRC/DST` on `!ptr` so `virtual_copy_f` routes through `vm_suspend` (VM faults the page in). Do NOT rely on `PHYS_COPY_CATCH` — a not-present dst makes the copy touch vaddr 0, whose caught fault addr `0` == the `if(addr)` "no fault" sentinel, so it silently writes nowhere (was THE exec-frame-copy bug; child got `ps_argvstr==0`). `createpde` returns `phys_to_kacc(phys)` (never 0) for present pages. Details: `docs/kernel-arch-x86_64.md` (Theme 2)

## exec / process setup (x86_64)
- `arch_proc_init(pr, ip, sp, ps_str, name)` params are `vir_bytes` (were `u32_t` — truncating on LP64)
- VFS computes `ps_str` authoritatively in `pm_exec` (`vsp + frame_len - sizeof(struct ps_strings)`) instead of echoing the caller's value — robust vs stale-libc callers and relocated frames
- amd64 crt0 ELF entry is asm `__start` (`lib/csu/arch/amd64/crt0.S`): `movq %rbx,%rdx; jmp ___start` — kernel passes `ps_strings` in `p_reg.bx` (RBX); the C `___start` reads it as its 3rd arg (RDX). Details: `docs/kernel-arch-x86_64.md`

## VM pt_free / pt_new (x86_64)
- `pt_new` allocates PML4+PDPT once and reuses them (recovering PDPT phys from the kept `pt_pml4[0]`); `pt_free` must therefore **keep `pt_pdpt`** (never free, like `pt_pml4`) and NULL the freed `pt_pd[]`/`pt_pt[]` slots — else the next `pt_new` `memset`s a freed page → "pagefault in VM" on exec slot reuse. Details: `docs/vm-x86_64-port.md`

## Kernel device-I/O calls (x86_64)
- `SYS_DEVIO`/`VDEVIO`/`SDEVIO`/`IOPENABLE`/`READBIOS` are needed on amd64 (same x86 port I/O as i386); register in `kernel/system.c` and build `do_devio.c`/`do_vdevio.c` in `kernel/system/Makefile.inc` under `__i386__ || __x86_64__`. Missing → "Unused kernel call 21" and tty/driver `sys_inb`/`sys_outb` failures. Details: `docs/kernel-arch-x86_64.md`

## PCI BAR addresses
- `pci_get_bar()` signature: `u64_t *base, u32_t *size` — callers must declare `u64_t base`; `pb_base` in `bus/pci/pci.c` is `u64_t`, `complete_bars()` skips BARs `> 0xFFFFFFFF`
- `PBF_INCOMPLETE` check must test the full 64-bit base `(bar | ((u64_t)bar_high << 32)) == 0` — testing only `bar == 0` wrongly marks a BAR at an exact 4 GB multiple as unallocated. Details: `docs/drivers-x86_64.md` (PCI BAR widening)

## Format strings — dev_t / ino_t / off_t (cross-arch)
- `dev_t` and `ino_t` are `uint64_t` = `unsigned long long` on i386, `unsigned long` on amd64
- `off_t` (`__off_t`) is `long long` on both arches
- Safe portable pattern: use `%llu`/`%llx` with `(unsigned long long)` cast, or `%lld` with `(long long)` cast — never bare `%lu`/`%ld` for these types (or use `PRIu64`/`PRIx64`)

## MMIO driver base widening (LP64)
- Full chain: struct `u32_t base[6]` → `vir_bytes base[6]`; `u32_t *base`/scalar params → `vir_bytes`; `io.h` `my_in*/my_out*` port params `u32_t` → `vir_bytes`; cast `(u32_t)reg` → `(vir_bytes)reg`. Audio drivers (als4000/cmi8738/cs4281/trident), ip1000, vt6105 all widened (also `.h` decls + `mixer.c` defs — three files per audio driver). Details: `docs/drivers-x86_64.md`

## phys_bytes vs u32_t pointer mismatch
- `phys_bytes` is `unsigned long`; `u32_t` is `unsigned int` — same 32-bit size on i386 but different types; `&u32_t_var` passed to `phys_bytes *` gives `-Wincompatible-pointer-types`
- Declare variables as `phys_bytes` (not `u32_t`) when passing to system APIs like `sys_vmctl_get_pdbr()`

## EFI linker script (MINIX)
- `elf_x86_64_minix_efi.lds` uses `ImageBase = 0`; do NOT add `FILEHDR PHDRS` to a PT_LOAD ("not enough room for program headers"). Correct PHDRS block: `{ text PT_LOAD; data PT_LOAD; dyn PT_DYNAMIC; }` (suppresses auto-PT_PHDR). Details: `docs/efiboot-bootx64-build.md` §9

## EFI bootloader (sys/stand/efiboot)
- Multiboot2 entry on EFI64: prefer `mpp_entry_elf64` (type-9, long mode) over `mpp_entry` (type-3, 32-bit PM) under `#ifdef __LP64__` — jumping to 32-bit code in long mode faults immediately
- `efi_md_init()` (sets up the `multiboot64`/`startprog64` trampolines) **must** be called from `efi_main()` (`efiboot.c`) before `boot()` — else `(*multiboot64)()` jumps through NULL right after ExitBootServices
- `efi_gop_found()` returns NULL on headless systems (null-check); `FreePages(addr, n)` takes a **page count** (use `EFI_SIZE_TO_PAGES`); IoAlign block alloc needs `blkbuf_size + IoAlign - 1`. Details: `docs/efiboot-bootx64-build.md`
- Boot bring-up debugging: a raw COM1 (0x3F8) byte writer (LCR=0x03 to clear DLAB, bounded LSR/THRE poll, `outb`) works before and after ExitBootServices and bypasses the EFI + kernel consoles — drop markers in `head.S`/loader to bisect a silent hang; the EFI `printf` is dead after ExitBootServices

## EFI64 boot bring-up (x86_64)
- Faults fixed for the first successful UEFI/multiboot2 boot: head.S `.code64` needs an explicit `.text`; boot identity map must cover low **4 GB**; `pg_identity()` must cover the LAPIC `0xFEE00000`/IOAPIC `0xFEC00000` MMIO hole; `prot_init` uses `x86_lldt(0)` (no LDT in long mode); `struct acpi_xsdt` must be `__packed`. Details: `docs/kernel-arch-x86_64.md` (EFI64 boot bring-up), `docs/apic-x86_64.md`

## Booting/testing under QEMU (x86_64 UEFI)
- **The user builds the live image and runs QEMU themselves** — do NOT rebuild the live image or launch QEMU. Build/install only the component(s) under change (`nbmake-amd64 -C <dir> [install]`), then hand off to the user to rebuild the image and boot; they paste back the serial/console log. When a fix needs runtime verification, prepare the change and tell the user what to test, rather than driving the boot
- Live image: `build/distrib/amd64/liveimage/emuimage/Minix-3.4.0-x86_64-live.img`; OVMF at `/usr/share/OVMF/OVMF_CODE_4M.fd` + a writable copy of `OVMF_VARS_4M.fd`
- Boot modules (kernel, tty, vm, etc. — see `boot.cfg` `multiboot2`/`load` lines) require rebuilding the live image to take effect; standalone servers/drivers (at_wini, ffs) can be swapped into the image's `/service`
- **q35 has no legacy IDE**: its disk is AHCI/SATA, so `at_wini` (legacy-IDE only, ports 0x1F0/0x170) fails IDENTIFY → root won't mount. Use a legacy-IDE disk (`-machine pc` + `-drive if=ide`) or the `ahci` driver (see `docs/drivers-x86_64.md`)
- `-serial file:LOG` captures kernel/driver output; `-no-reboot` makes a triple-fault/panic exit QEMU (vs reboot loop)
- Background QEMU via the Bash tool reports "completed" while the VM keeps running → stale procs hold the image write-lock; `pkill -9 qemu-system-x86` and use unique image/serial/vars filenames per run

## Debugging crashes (x86_64)
- Symbolize a panic/coredump stack (`vm 8 0x..`, `kernel on CPU 0: 0x..`, `<prog> <pid> 0x..`) with `$TOOLDIR/bin/x86_64-elf64-minix-addr2line -f -e <binary> <hex...>` — servers/userland are static non-PIE (text at 0x400000) so raw addresses map directly; gives function names even with line info stripped
- Binaries: kernel `build/destdir.amd64/usr/sbin/kernel` (symbols via `nm kernel`, not `kernel.debug`), servers `build/destdir.amd64/service/<name>`, tools in `destdir.amd64/usr/{bin,sbin}`
- Do NOT hand-roll `nm`+`awk` symbolization: awk's doubles lose precision on 64-bit kernel addresses (`0xffffffff8...`)
- LP64 crash signatures: `0xffffffffffffffff`/`...fffe` = a sign-extended `int` used as a 64-bit value (missing prototype, or `int` return into pointer/`phys_bytes`); a small addr like `0x17` = NULL-struct base + field offset
- Read-back-verify catches bad pages at the source: a temporary "write a sentinel, read it back" check in `vm_memset` (kernel) / `vm_allocpages` (VM) / `alloc_pages` pinpointed a non-RAM ROM page and a garbage page number during the boot-to-login corruption hunt

## Console routing (x86_64) — serial vs framebuffer
- `tty/arch/x86_64/console.c scr_init` picks the console backend by `kinfo.boot_mode`: UEFI (1) → `fb_cons_sw` (framebuffer), else `ser_cons_sw`. amd64 has no `bios_console.c` (i386-only, BIOS text mode)
- `etc/ttys`/`etc.amd64/ttys` run getty on `console` (=ttyc0=framebuffer under UEFI), with `tty00` (serial) **off** → **rc/login output goes to the GFX window, not serial**
- Serial only carries **kernel/driver `printf`** (kernel console = `consdev=com0`); a quiet serial after driver init usually means userland is live on the framebuffer, not a hang
- `fb_console.c` has `DUP_CONS_TO_SER=1` (mirrors console→serial); i386 `bios_console.c` has it `0`
- `fb_console.c out_char` must call the vt100 emulator with the `kernel` flag **0** for userland tty output/echo — `wsemul_vt100_output` drops `ESC` and forces every byte through `output_normal` when `kernel!=0`, so escape sequences (`\E[31m` colors, `\E[K` clr-to-eol, cursor moves, `vi`) print literally as `[31m`/`[K`. Only the kernel-console path `wsdisplay_cnputc` uses `kernel=1`. Was wrong in both arch `fb_console.c`; surfaced first as Backspace showing `[K`
- Console TERM is **`wsvt25`** (set in `etc/ttys`+`etc/rc.minix`), not `vt100`: keyboard.c emits CSI-form arrows (`\E[A`) + function keys (`\E[11~`), matching wsvt25/vt220 terminfo; the `vt100` entry's `\EOA`/`\EOP` (application-mode) would break arrows/F-keys. `wsvt25` resolves via `/usr/share/terminfo/terminfo.cdb`, not `/etc/termcap`

## VM pagetable PTF flags
- `PTF_ALLFLAGS` (`vm/arch/x86_64/pagetable.h`) must include every PTF_ flag callers may pass — `assert(!(flags & ~PTF_ALLFLAGS))` in `pt_writemap` rejects unknown flags
- `PTF_NOEXEC` (bit 63 = `AMD64_VM_NX`) must NOT be set in a PDE — a PDE with NX=1 blocks instruction-fetch from all 512 PTEs under it; strip it in `pt_ptalloc`. The full NX chain must be `u64_t` throughout (`pt_flags` callback in `memtype.h`, all six `mem_*.c`, `region.c:map_ph_writept`, `kern_mappings[].flags`) — any `int` truncates bit 63. Details: `docs/vm-x86_64-port.md`

## SMP AP startup (x86_64)
- `copy_trampoline()` (`arch_smp.c`) must copy `idt` (not `gdt`) into `__ap_idt_tab` — the wrong source fills the AP IDT with segment descriptors → triple-fault on the first interrupt on every AP
- Descriptor sizes differ: GDT segment descriptor = 8 B (`DESC_SIZE`); long-mode IDT gate = 16 B (`GATE_DESC_SIZE`); `desctableptr_s` = 10 B packed. Trampoline `.space` reservations must use the right one
- `cpuid` macro must use `(vir_bytes)` arithmetic and read a `reg_t` (`[-1]` on a `reg_t *`), not i386's `u32_t`; any kernel pointer passed between CPUs (e.g. `sched_ipi_data.data`) must be `vir_bytes`, never `u32_t` — these are latent under UP (`#define cpuid 0`). Details: `docs/kernel-arch-x86_64.md` (SMP)
- `phys_copy` on amd64 is raw `rep movsb` in the **current** address space (no phys→kernel-virt translation); after VM loads it splits/removes the identity mapping of the kernel's own physical region (0x400000–0x600000, see `tss_init` comment), so `phys_copy(x, vir2phys(&kernel_var), n)` faults — the kernel-region physical alias is unmapped. Pass the kernel **VA** directly (`(phys_bytes)&var`), never `vir2phys()` of a kernel address. Was the `apic_send_init_ipi` warm-reset-vector bug (faulted writing `phys(stack_local)` → reboot at "SMP initialized"); `smp_start_aps`'s own `0x467` copies already use the VA idiom. Low (<1 MB) and process addresses are fine — their identity pages aren't split
- `runqueues_ok_cpu(cpu)` (`debug.c`, only live under `DEBUG_SANITYCHECKS=1`, which this tree sets) walks just `cpu`'s per-CPU run queues; its "ready proc N not on queue" loop must filter by `xp->p_cpu == cpu` or it false-positives on procs queued on another CPU. Latent stock-MINIX bug, exposed only by SMP. Asserts via `runqueues_ok_local()` = `runqueues_ok_cpu(cpuid)` in `enqueue`/`dequeue`

## ACPI XSDT (x86_64)
- UEFI x86_64 is ACPI revision 2: RSDP → **XSDT** (64-bit table addresses); `acpi.c` branches on `acpi_rsdp.revision` (0 = RSDT, 2 = XSDT). Keep table addresses full-width `phys_bytes` (the old `(u32_t)xsdt.data[i]` truncated tables >4 GB); resolved address stored in `sdt_trans[i].base` (single source of truth)
- The RSDP reaches the kernel via the multiboot2 ACPI2 tag (`kinfo.rsdp_p`) on EFI — the legacy EBDA/BIOS scan finds nothing under pure UEFI. `mb2_acpi2.c` was unused and is deleted. Details: `docs/apic-x86_64.md`

## FFS / UFS2 filesystem (minix/fs/ffs, minix/sbin/{newfs,fsck}_ffs)
- NetBSD `sys/ufs/ffs` on-disk format (UFS2 only, native little-endian) reimplemented as a MINIX userspace FS server (libfsdriver `fsdriver_task` + libminixfs `lmfs_*`), modeled on `minix/fs/ext2` — NOT a build of the kernel `sys/ufs` code
- Block size = **fragment** (`lmfs_set_blocksize(fs->fs_fsize)`); UFS frag/block/cg addresses map 1:1 onto cache block numbers; VM second-level cache is disabled (`lmfs_may_use_vmcache(0)`) so fragment relocation stays coherent
- `read_super` does the full `SBLOCKSEARCH` and accepts both `FS_UFS2_MAGIC` and `FS_UFS2EA_MAGIC` (`nbmakefs -o version=2` writes 8192/UFS2EA; standard `newfs`/our `newfs_ffs` write 65536/UFS2)
- `ffs_isblock` (`==0xff`, "already free") vs `ffs_isfreeblock` (`==0`, "fully allocated") have *opposite* meaning — the whole-block double-free guard in `ffs_blkfree` must use `ffs_isblock`
- `mount -t ffs` needs no `mount_ffs` binary (libc `minix_mount` execs `/service/ffs`); host regression test in `minix/fs/ffs/test/` (`sh run.sh`) runs the real server source over an image file. Details: `docs/ffs-ufs2-port.md`

## Port documentation (docs/)
- Per-subsystem x86_64 port notes live in `docs/*.md` (e.g. `kernel-arch-x86_64.md`, `apic-x86_64.md`, `vm-x86_64-port.md`, `drivers-x86_64.md`, `ipc-abi-x86_64.md`, `userland-x86_64-port.md`, `efiboot-bootx64-build.md`, `ffs-ufs2-port.md`, `sets-amd64-port.md`); add new kernel/SMP findings to `docs/kernel-arch-x86_64.md`, matching its before/after code-block style

## /code-review on this repo
- No GitHub PRs exist (`origin` = github.com/nnonaka/minix); review the diff against base branch `uefiboot` (= `origin/uefiboot`), or review a subsystem holistically when the topic is broader than the diff
- Local `HEAD` is typically NOT pushed — for GitHub citation links use the latest pushed SHA (`git branch -r --contains <sha>` to confirm; usually `origin/uefiboot`), not local HEAD, or links 404

## Distribution sets
- New arch port: create `distrib/sets/lists/{minix-base,minix-comp,minix-debug}/md.<machine>` (`md.i386` is the reference). Set format `./path  set-name  [tag]`; tagged entries (e.g. `binutils`, `nls`, `llvmcmds`) are included only when the matching `MK*` var is in `MKEXTRAVARS` (`distrib/sets/mkvars.mk`) and not "no"
- `MKEXTRAVARS` was missing `MKBINUTILS`/`MKLLVM`/`MKLLVMCMDS`/`MKNLS` → ldscripts/clang-headers/locale silently omitted. Use `SLOPPY_FLIST=YES` (extras OK, missing fatal). Details: `docs/sets-amd64-port.md`
