# MINIX kernel/arch/x86_64 — x86_64 port notes

Previously a byte-for-byte copy of `kernel/arch/i386`. This document describes
the changes made to produce a native 64-bit (AMD64 SysV) kernel for the
`dev-efi3` branch.

## Boot flow

### Entry points

Two entry paths are defined in `head.S`:

| Path | How invoked | Starting mode |
|---|---|---|
| `multiboot_entry32` | BIOS/legacy multiboot2 bootloader | 32-bit protected mode |
| `multiboot_entry64_efi` | UEFI bootloader with EFI64 entry tag (type 9) | 64-bit long mode |

Both paths converge at `.Lentry64_common` in 64-bit mode with boot page
tables active.

### 32-bit → 64-bit transition (multiboot_entry32)

1. Save `EAX`/`EBX` (magic / info pointer) to `.data`.
2. Build boot page tables (PML4 + PDPT + PD) in `.bss`:
   - `PML4[0] → pdpt_low[0] → pd_low[0..7]`: identity-maps 0–16 MB (2 MB pages).
   - `PML4[511] → pdpt_high[510] → pd_kern[2..9]`: maps kernel at −2 GB.
3. Load `boot_gdtptr32` (6-byte GDTR for 32-bit mode).
4. Set `CR4.PAE`, load `CR3`, set `EFER.LME`, set `CR0.PG` → IA-32e compatibility mode.
5. Far jump `ljmpl $KERN_CS_SELECTOR, $.Lentry64_from32` — the target is the
   physical address of the label (unpaged section links at `_kern_phys_base`),
   which is identity-mapped.  Zero-extending the 32-bit offset gives the correct
   64-bit RIP.

### EFI-64 path (multiboot_entry64_efi)

1. Save magic/info via RIP-relative stores.
2. Load `boot_gdtptr64` (10-byte GDTR) via RIP-relative `lgdtq`.
3. Reload CS to our 64-bit segment via the far-return trick
   (`pushq $KERN_CS_SELECTOR; pushq %rip_target; lretq`).
4. Rebuild boot page tables using 64-bit RIP-relative writes (same layout as above).
5. Switch `CR3` to `boot_pml4`.
6. Fall through to `.Lentry64_common`.

### Common 64-bit entry (.Lentry64_common)

- Zero DS/ES/SS.
- Switch to 64-bit boot stack (`boot_stack64`).
- Reload magic/info from memory (handles both paths uniformly).
- Call `pre_init(u32_t magic, u32_t info_ptr)` — AMD64 ABI: args in rdi, rsi.
- On return, `rax` = `kinfo_t *`.
- Switch RSP to `k_initial_stktop` (high-virtual kernel stack, now mapped).
- Indirect call to `kmain` via `movabsq` (avoids 32-bit displacement overflow
  from physical to high-virtual address).

## Memory layout

```
_kern_phys_base = 0x00400000   (physical load address)
_kern_vir_base  = 0xFFFFFFFF80400000   (kernel at −2 GB)
_kern_offset    = _kern_vir_base − _kern_phys_base = 0xFFFFFFFF80000000
```

Linker script (`kernel.lds`) places the unpaged section first at the physical
base, then jumps the VMA by `_kern_offset` for all normal sections:

```
. = 0x00400000;
.unpaged_text / .unpaged_data / .unpaged_bss   ← physical addresses

. += _kern_offset;   → VMA now 0xFFFFFFFF80400000 + phys_offset

.usermapped_glo / .usermapped / .text / .data / .bss   ← high virtual
```

## Page tables

`pg_utils.c` implements 4-level paging (PML4 → PDPT → PD → PT).

| Structure | Covers |
|---|---|
| `pg_pml4[512]` | root |
| `pg_pdpt_low[512]` | PML4[0] — low canonical half |
| `pg_pdpt_high[512]` | PML4[511] — high canonical half |
| `pg_pd_ident[64][512]` | identity 2 MB pages, up to 64 GB |
| `pg_pd_kern[512]` | kernel 2 MB pages at −2 GB |
| `pagetables[32][512]` | 4 KB page tables for `pg_map()` |

`pg_identity()` fills PML4[0]→pdpt_low→pd_ident[i] with 2 MB PS pages.
`pg_mapkernel()` fills PML4[511]→pdpt_high[510]→pd_kern starting at PD index 2
(which corresponds to virtual address `0xFFFFFFFF80400000`).

`pg_map()` is used by `arch_boot_proc()` to map VM into the bootstrap page
table using 4 KB pages.  It only handles user-space addresses (PML4 index 0).

## GDT / IDT / TSS

### Code segments

64-bit code segments require `L=1` and `D=0` (Intel SDM §3.4.5).
`init_codeseg()` calls `sdesc()` then clears `DEFAULT` and sets `LONG_MODE`.

### IDT gates

64-bit interrupt gates are 16 bytes.  `struct gatedesc_s` in `archtypes.h`:

```c
u16_t offset_low;
u16_t selector;
u8_t  ist;
u8_t  p_dpl_type;
u16_t offset_mid;
u32_t offset_high;
u32_t reserved;
```

### TSS

The 64-bit TSS descriptor occupies **two consecutive GDT slots** (16 bytes total).
`TSS_INDEX(cpu) = TSS_INDEX_FIRST + cpu * 2`.

`tss_init()` writes:
- Slot N: standard 8-byte descriptor (base[31:0], limit, type=`TSS_TYPE`).
- Slot N+1: low 32 bits = base[63:32], rest zero.

`tss_s` (in `arch_proto.h`) follows the Intel SDM Vol. 3A §7.7 64-bit layout:
`reserved0`, `rsp0`–`rsp2`, `reserved1`, `ist[7]`, `reserved2`, `reserved3`,
`iobase`.  No legacy segment-register fields (`ds`, `es`, `ss0`, `sp0`, etc.).

## SYSCALL / SYSENTER MSR setup (setup_sysenter_syscall)

On x86_64 the `SYSCALL` instruction reads RIP from **LSTAR** (MSR `0xC0000082`),
not from STAR's low 32 bits.

- `AMD_MSR_STAR` bits[63:32] = `(USER_CS_SELECTOR << 16) | KERN_CS_SELECTOR`;
  bits[31:0] = 0 (unused for 64-bit SYSCALL).
- `AMD_MSR_LSTAR` = full 64-bit virtual address of `ipc_entry_syscall_cpuN`,
  written as hi/lo pair through `ia32_msr_write`.
- `INTEL_MSR_SYSENTER_ESP` / `EIP` are also written as full 64-bit values
  (hi/lo), not truncated to 32 bits.
- `AMD_MSR_FMASK` (`0xC0000084`) **must** be programmed. `SYSCALL` clears
  exactly the RFLAGS bits set in FMASK; left at its power-on value of 0 it
  clears nothing, so the user-space `IF` (interrupts enabled) carries straight
  into the kernel and the **entire syscall/IPC path runs with interrupts on**.
  A device IRQ can then `mini_notify()` → `enqueue()` a process while the kernel
  is walking/editing the run queues, corrupting them. i386 is immune because it
  enters through an INT/interrupt gate, which clears `IF` in hardware.

```c
/* before: FMASK never written -> IF stays set through SYSCALL */

/* after (setup_sysenter_syscall): mask NT|AC|DF|IF|TF like NetBSD/amd64 */
ia32_msr_write(AMD_MSR_FMASK, 0, 0x44700);
/*   NT 0x4000 | AC 0x40000 | DF 0x400 | IF 0x200 | TF 0x100 */
```

  `SYSRET` restores RFLAGS from `r11` (which holds the user value with `IF`
  set), so interrupts resume on return to user. Symptom before the fix: the
  `runqueues_ok` "double sched" assert (`proc.c` `dequeue`/`enqueue`) firing
  intermittently once `DEBUG_SANITYCHECKS` was enabled — a false-looking
  double (proc linked once but `p_found` already set) because an IRQ moved a
  process between queues mid-check. With the fix the system boots to multiuser.

## klib.S — AMD64 ABI register conventions

All functions follow the AMD64 SysV ABI:

| Purpose | Registers |
|---|---|
| Integer arguments | rdi, rsi, rdx, rcx, r8, r9 |
| Return value | rax |
| Callee-saved | rbx, rbp, r12–r15 |
| Clobbered by CPUID | rbx, rcx, rdx — saved/restored in smp_get_* |

Notable implementations:

- **phys_insw/insb**: `insw`/`insb` use dx=port, rdi=dest, rcx=count.  On entry
  rdi=port, rsi=buf, rdx=count, so the function swaps them before the rep.
- **phys_copy**: `rep movsb` uses rsi=src, rdi=dst; the function swaps the first
  two arguments.
- **x86_load_kerncs**: uses the far-return trick (`pushq $CS; pushq $ret_addr; lretq`).
- **ia32_msr_read**: saves rdx (third arg = lo ptr) to r8 before `rdmsr` clobbers rdx.
- **ia32_msr_write**: rdi=reg, rsi=hi, rdx=lo → ecx=reg, edx=hi, eax=lo before `wrmsr`.

## Unpaged section and objcopy prefix mechanism

Objects in the unpaged list (head.o, pre_init.o, pg_utils.o, klib.o, mpx.o, …)
are processed with `objcopy --prefix-symbols=__k_unpaged_`, which renames **all**
symbols (both defined and external references).

Cross-object references from unpaged to non-unpaged code require explicit
`__k_unpaged_` aliases:

- `mpx.S` defines `LABEL(__k_unpaged_k_initial_stktop)` at the physical stack top.
- `kernel.lds` provides `PROVIDE(__k_unpaged_kmain = kmain)` so that the
  prefixed `call __k_unpaged_kmain` in `unpaged_head.o` resolves to the
  high-virtual address of `kmain` (which is accessible once `pre_init` has
  installed page tables).

## Multiboot2 header

```
magic    = 0xe85250d6
arch     = 0 (MULTIBOOT2_ARCHITECTURE_I386 — 32-bit PM entry convention)
checksum = -(magic + arch + header_size)
```

Tags (all 8-byte aligned):

| Tag | Type | Notes |
|---|---|---|
| Information request | 1 | cmdline, module, mmap, framebuffer, EFI mmap |
| Entry address | 3 | → `multiboot_entry32` |
| EFI64 entry address | 9 | → `multiboot_entry64_efi` |
| End | 0 | — |

The information-request tag `size` field is 28 (8-byte header + 5 × 4-byte
request fields), **not** including inter-tag alignment padding.

## SMP AP trampoline (`trampoline.S`)

APs start in 16-bit real mode and must reach the 64-bit `startup_ap_32` entry
point in `mpx.S` (the name is misleading — it is 64-bit code).  The transition
sequence:

1. In 16-bit real mode: compute the physical address of `__ap_startup_64`
   (the 64-bit stub within the trampoline page) as `CS × 16 + offset` and
   store it in the `__ap_jmpvec` far-pointer slot.
2. Load the BSP-prepared GDT (copied into `__ap_gdt_tab` by `copy_trampoline`)
   and IDT via `lgdtl` / `lidtl`.
3. Set `CR0.PE` — enter 32-bit protected mode.
4. Set `CR4.PAE` — required for long mode.
5. Load the BSP's PML4 physical address from `__ap_pt` into `CR3`.
6. Set `EFER.LME` via `wrmsr` on MSR `0xC0000080`.
7. Set `CR0.PG` — paging on; CPU enters IA-32e compatibility mode.
8. `ljmpl *__ap_jmpvec` — reads the 6-byte far pointer
   `{ phys_addr_of___ap_startup_64, KERN_CS_SELECTOR }`.  Loading the 64-bit
   CS descriptor transitions the CPU to 64-bit long mode; RIP = physical
   address of `__ap_startup_64` (identity-mapped, so VA = PA).
9. **`__ap_startup_64`** (64-bit stub in the trampoline page): `movabs
   $startup_ap_32, %rax; jmpq *%rax` — loads the full 64-bit kernel VA and
   jumps there.

The `ljmpl` must encode a 32-bit offset that fits in the low-memory trampoline
page; it cannot encode the high-kernel VA of `startup_ap_32` directly.  The
two-step approach (stub in trampoline → `movabs` to high VA) is the standard
x86_64 SMP solution.

### arch_smp.c — AP IDT table copy bug *(fixed)*

`copy_trampoline()` prepares the GDT and IDT for APs before copying the
trampoline to low memory.  The original code had a copy-paste error:

```c
memcpy(&__ap_gdt_tab, gdt, sizeof(gdt));
memcpy(&__ap_idt_tab, gdt, sizeof(idt));  /* BUG: gdt instead of idt */
```

The second `memcpy` used `gdt` as the source instead of `idt`, so every AP
received GDT descriptors in its IDT slot.  The first exception or external
interrupt on any AP (timer, NMI, page-fault) resolved the vector number
through a GDT entry, produced a garbage CS:EIP gate, and triggered an
immediate triple-fault.  SMP never became functional: all APs would die on
their first interrupt.

**Fix** (`arch_smp.c:90`): source changed to `idt`.

```c
memcpy(&__ap_idt_tab, idt, sizeof(idt));
```

### LP64 porting bugs in the SMP path *(fixed)*

The generic SMP core and the AP trampoline data layout were copied verbatim
from i386, where pointers and the gate/pointer descriptors happen to be
32-bit / 8-byte / 6-byte.  On x86_64 (LP64, long-mode descriptors) four of
those assumptions break.  All four are latent until `CONFIG_SMP` is enabled —
a UP kernel uses `#define cpuid 0` from `kernel.h` and never sends scheduling
IPIs — so they did not surface during single-CPU bring-up.

#### 1. `cpuid` macro truncates the kernel stack VA (`arch_smp.h`)

```c
/* before — correct on i386 (pointer == u32_t), broken on x86_64 */
#define cpuid (((u32_t *)(((u32_t)get_stack_frame() + (K_STACK_SIZE - 1)) \
                          & ~(K_STACK_SIZE - 1)))[-1])
```

`get_stack_frame()` returns a `reg_t` (64-bit) pointing into the kernel stack
at `0xFFFFFFFF80xxxxxx`.  The `(u32_t)` cast truncated it to `0x80xxxxxx`,
which dereferences to an invalid low VA.  Separately, `tss_init()` stores the
cpu id as a full `reg_t` at `kernel_stack_top - sizeof(reg_t)`
(`protect.c`: `*((reg_t *)(rsp0 + 1*sizeof(reg_t))) = cpu`), but the macro
read a `u32_t` at `stack_top - 4` — the high (always-zero) half of that slot.
So every CPU evaluated `cpuid == 0`, folding all per-CPU TSS, scheduling, and
cycle-accounting state onto CPU 0.

```c
/* after — vir_bytes arithmetic (no truncation), reg_t read (full 8-byte slot) */
#define cpuid (((reg_t *)(((vir_bytes)get_stack_frame() + (K_STACK_SIZE - 1)) \
                          & ~(K_STACK_SIZE - 1)))[-1])
```

`reg_t *` with `[-1]` reads `stack_top - 8`, matching where `tss_init` writes.

#### 2. `sched_ipi_data.data` truncates a kernel pointer (`smp.c`)

The scheduling-IPI mailbox passed a `struct proc *` between CPUs through a
`u32_t` field:

```c
struct sched_ipi_data {
        volatile u32_t flags;
        volatile u32_t data;          /* before: truncates a 64-bit proc ptr */
};
...
sched_ipi_data[cpu].data = (u32_t) p;             /* loses high 32 bits */
p = (struct proc *)sched_ipi_data[cpu].data;      /* zero-extends → bad addr */
```

`struct proc` objects live in the kernel image at `0xFFFFFFFF80xxxxxx`.  The
receiver reconstructed `0x0000000080xxxxxx` and dereferenced it in
`smp_sched_handler` on the first stop/vm-inhibit/migration IPI.  Fix: widen
the field to `vir_bytes` and store `(vir_bytes) p`; the readback already casts
back to `struct proc *`.

#### 3. AP IDT trampoline buffer was half-size (`trampoline.S`)

```asm
LABEL(__ap_idt_tab)
.space IDT_SIZE*DESC_SIZE        /* before: 256 * 8 = 2048 bytes */
```

`DESC_SIZE` (8) is the *segment* descriptor size, correct for the GDT
(`struct segdesc_s`).  But long-mode *gate* descriptors (`struct gatedesc_s`,
the IDT element type) are 16 bytes, so `sizeof(idt) = 256 * 16 = 4096`.
`copy_trampoline()`'s `memcpy(&__ap_idt_tab, idt, sizeof(idt))` therefore
overflowed the 2048-byte buffer by 2048 bytes into adjacent trampoline memory
on every AP boot.  Fix: add a `GATE_DESC_SIZE` (16) macro in `archconst.h`
and reserve `IDT_SIZE*GATE_DESC_SIZE`.

**i386 does NOT have this buffer-size bug** — i386 IDT gates *are* 8 bytes
(`DESC_SIZE`), so `IDT_SIZE*DESC_SIZE` = 2048 correctly sizes `__ap_idt_tab`.
But i386 `copy_trampoline()` had a *different* defect in the same spot:
`memcpy(&__ap_idt_tab, gdt, sizeof(idt))` copied from **`gdt`** instead of `idt`
(the same wrong-source bug amd64's `arch_smp.c` also had originally — see the
CLAUDE.md "SMP AP startup" note).  It filled every AP's IDT with GDT segment
descriptors (and read ~2 KB past the small GDT), triple-faulting each AP on its
first interrupt.  Fix: `memcpy(&__ap_idt_tab, idt, sizeof(idt))`.  Latent until
i386 actually brings APs up.

#### 4. Descriptor-table pointer slots were 8 bytes, not 10 (`trampoline.S`)

`__ap_gdt` / `__ap_idt` hold `struct desctableptr_s` = `{ u16 limit; u64 base; }`
= 10 bytes packed, but were reserved as `.space 8`.  Writing `.base` overflowed
2 bytes into the next field.  This was *benign in practice* — the overflow
bytes are the zero high half of a sub-1 MB physical address, and the clobbered
fields (`__ap_idt.limit`, `__ap_jmpvec`) are rewritten afterward — but fragile.
Reserved `.space 10` for each to match the struct.

#### 5. `trampoline.o` rule piped through the broken external `as` (`Makefile.inc`)

`trampoline.S` is the only kernel source with its own build rule, because its
real-mode (`.code16`) AP entry historically needed GNU `as`.  The rule
preprocessed with clang and piped the result to `${AS}`
(`x86_64-elf64-minix-as`):

```make
# before — pipe to external as
trampoline.o: trampoline.S
	${CC} -E ${AFLAGS} ${AFLAGS.${<:T}} ${CPPFLAGS} ${.IMPSRC} | ${AS} -o ${.TARGET}
```

The NetBSD binutils `as` (2.34) for this target was built **without the
`elf64-x86-64` BFD vector** — even a trivial `nop` fails with
`Fatal error: selected target format 'elf64-x86-64' unknown`.  Every other
kernel `.S` builds fine because the default suffix rule uses clang's
integrated assembler, which also handles `.code16` correctly.  Fix: assemble
`trampoline.S` directly with clang, dropping the external `as`:

```make
# after — clang integrated assembler (handles .code16)
trampoline.o: trampoline.S
	${CC} ${AFLAGS} ${AFLAGS.${<:T}} ${CPPFLAGS} -c ${.IMPSRC} -o ${.TARGET}
```

#### 6. `cpuid` printed with `%d` (`-Werror=format`)

Once `CONFIG_SMP` is set, `cpuid` expands to a `reg_t` (`unsigned long`) read
from the kernel stack (see #1), but several `printf`s carried it with `%d`.
On i386 `cpuid` is a `u32_t`, so `%d` was correct there; on x86_64 clang
rejects it under `-Werror,-Wformat`.  Cast to `(int)cpuid` (cpu ids are small)
in `utility.c` (`kernel on CPU %d` in `panic`), `apic.c` (spurious/error
interrupt warnings), and `proc.c` (two `TRACE` scheduling prints).  These are
the only SMP-only format errors; UP builds never reach them because `cpuid` is
a literal `0`.

#### 7. Off-by-one IDT write corrupts `k_percpu_stacks[0]` → SYSCALL triple-fault *(the SMP boot-to-login bug)*

This was the bug that stopped `cpunum=1` from reaching `login:`: the kernel
booted all the way through VM and the servers, then **silently rebooted the
instant userland issued its first `SYSCALL`-based IPC** (no panic, no `DBG exc`
— a pure triple-fault).

The decisive clue was that booting with `libc_ipc=1` (which makes the kernel
*not* publish the SYSCALL ipcvecs, so libc falls back to the `INT $vec` IPC
path) booted to login fine.  So the fault was specific to the **SYSCALL fast
path**, which differs from the INT path in exactly one way that matters here:

```asm
/* ipc_entry_syscall_cpuN (mpx.S): SYSCALL does NOT switch the stack, so the
 * stub loads the per-CPU kernel stack itself, from the C array: */
	movq	k_percpu_stacks + 8*cpu, %rsp
```

The INT path instead gets its stack from `tss[cpu].rsp0` (the CPU loads it from
the TSS on a ring3→ring0 gate).  Tracing `setup_sysenter_syscall()` showed
`k_percpu_stacks[0]` flip from a valid `0xffffffff804ffff0` to garbage
(`0x804d8e0000086262`) between two calls, while `tss[0].rsp0` stayed valid —
hence INT survived, SYSCALL loaded a garbage `%rsp` and triple-faulted on its
first push.

Decoding the garbage as little-endian bytes revealed it was an **IDT gate
descriptor** (offset `0x804d6262` = `_lapic_intr_dummy_handler_255`, selector
`0x0008` = `KERN_CS`, type `0x8E` = present interrupt gate).  The IDT
(`256 * 16` = 4096 bytes) sits **immediately below `k_percpu_stacks` in BSS**,
so `idt[256]` *is* `k_percpu_stacks[0]`.  Something wrote one gate past the end
of the 256-entry table.

Root cause is in `lapic_set_dummy_handlers()` (`apic.c`, only built/run when
`APIC_DEBUG` is on).  Each `LAPIC_INTR_DUMMY_HANDLER(n)` does its own
`.balign LAPIC_INTR_DUMMY_HANDLER_SIZE`, but the surrounding
`LABEL(lapic_intr_dummy_handles_start)` did **not** — so the label landed ~30
bytes *before* handler 0:

```
lapic_intr_dummy_handles_start = 0x...4262   (label, unaligned)
_lapic_intr_dummy_handler_0    = 0x...4280   (.balign 32 -> 0x1E later)
```

The loop computes positions as `start + vect*SIZE` and bounds itself by
`handler < &lapic_intr_dummy_handles_end`.  With `start` 30 bytes low, the bound
allows **one extra iteration**, pushing `vect` to `256` and calling
`int_gate_idt(256, …)` → write past the IDT into `k_percpu_stacks[0]`.

Fix (`apic_asm.S`): align the label to the handler stride so it coincides with
handler 0:

```asm
.balign LAPIC_INTR_DUMMY_HANDLER_SIZE
LABEL(lapic_intr_dummy_handles_start)
	LAPIC_INTR_DUMMY_HANDLER(0)
```

Defensive bound (`apic.c`), so an out-of-range vector can never write past the
IDT again regardless of label alignment:

```c
for(; handler < &lapic_intr_dummy_handles_end && vect < IDT_SIZE; …)
```

Two related x86_64 SYSCALL-path corrections made while hunting this:

- **`mpx.S`** — the per-CPU SYSCALL stub now runs `RESTORE_KERNEL_SEGS` before
  entering C, matching the INT path's `SAVE_PROCESS_CTX`.  SYSCALL leaves the
  user data-segment selectors loaded; the stub must reset them like every other
  kernel entry.
- **`arch_proto.h`** — `K_STACK_SIZE` raised from one page to `4 * I386_PAGE_SIZE`
  (16 KB).  It doubles as the rounding granularity for the `cpuid` macro (see
  #1), which is only valid while the stack pointer stays within the top
  `K_STACK_SIZE` of `get_k_stack_top(cpu)`.  x86_64 kernel frames are ~2x i386's,
  so a single 4 KB page is uncomfortably tight for deep chains (e.g.
  `RECEIVE(ANY)` with async delivery); 16 KB (NetBSD/amd64 UPAGES) keeps the
  cpuid-safe window well clear of real usage.

Lesson: any fixed-size table immediately followed by a live global in BSS turns
an off-by-one write into silent corruption of unrelated state.  The IDT/
`k_percpu_stacks` adjacency made a debug-only installer fatal — and only on the
SYSCALL path, because that is the sole consumer of `k_percpu_stacks`.

**i386 has the identical bug** (the `apic_asm.S` label, the `apic.c` loop, the
IDT/`k_percpu_stacks` BSS adjacency are all mirror images of amd64).  It only
surfaces where the firmware actually offers the AMD SYSCALL fast path — e.g.
VirtualBox in 32-bit legacy mode (`kernel: selecting amd syscall ipc style`),
where QEMU/Intel would have used SYSENTER and dodged it.  The i386 stub is
`ipc_entry_syscall_cpuN` in `arch/i386/mpx.S` (`mov k_percpu_stacks+4*cpu,%esi;
mov (%esi),%ebp`); the gate is 8 bytes so `idt[256]` lands on
`k_percpu_stacks[0]` exactly (`idt 0xf0470330 + 256*8 = 0xf0470b30`).  The 257th
write stored gate dword0 `(KERN_CS<<16)|low16(handler256)` = `0x0008b5a5` there,
and the first user SYSCALL faulted dereferencing `%esi = 0x8b5a5` in
`ipc_entry_syscall_cpu0` (not a triple-fault on i386 — a clean
`pagefault in kernel ... address 0x8b5a5`, `vec_nr=14`).  Same two-part fix
ported: `.balign LAPIC_INTR_DUMMY_HANDLER_SIZE` before the start label, and the
`&& vect < IDT_SIZE` loop guard.  (i386 `K_STACK_SIZE` stays one page — the
16 KB bump above is an amd64-frame-size need, not part of this fix.)

#### 8. `apic_send_init_ipi` `phys_copy(vir2phys(&stack_local))` faults *(reboot at "SMP initialized")*

After #7, the BSP reached `smp_start_aps` but rebooted the instant it started
the first AP — a nested kernel page fault writing to `0x4fff9c`:

```
SMP initialized
pagefault in kernel at pc 0x...4c57e7 (phys_copy) address 0x4fff9c
   ... apic_send_init_ipi -> phys_copy   (nested, write, not-present)
CPU 1 didn't boot
```

`0x4fff9c` is the **physical** alias of a live BSP-stack slot
(`0xffffffff804fff9c`).  The warm-reset-vector setup in `apic_send_init_ipi`
(`apic.c`) copied straight from i386:

```c
u32_t ptr;
ptr = (u32_t)(trampoline & 0xF);
phys_copy(0x467, vir2phys(&ptr), sizeof(u16_t));   /* dst = phys(stack local) */
```

amd64 `phys_copy` is a raw `rep movsb` in the **current** address space (it does
*not* translate phys→kernel-virt the way some ports do).  `vir2phys(&ptr)`
yields the stack local's physical address inside the kernel image's
`0x400000–0x600000` range — and once VM loads it **splits/removes the identity
mapping of that 2 MB region** (the same effect documented for the TSS in
`protect.c`), so the physical alias is no longer mapped.  i386 runs the identical
line harmlessly only because it keeps the kernel region identity-mapped (and APs
boot via the SIPI vector regardless of the warm-reset bytes).

Fix (`apic.c`): pass the kernel **virtual** address directly — the idiom
`smp_start_aps` already uses for its own `0x467` copies (`phys_copy(0x467,
(phys_bytes)&biosresetvector, …)`), which is why those succeeded:

```c
phys_copy(0x467, (phys_bytes) &ptr, sizeof(u16_t));
phys_copy(0x469, (phys_bytes) &ptr, sizeof(u16_t));
```

Source `0x467` (sub-1 MB) stays identity-mapped; dest `&ptr` is a normal mapped
kernel VA.  Rule: on amd64 never feed `vir2phys()` of a *kernel* address to
`phys_copy` — use the VA.  Low (<1 MB) and process physical addresses are fine;
only the kernel's own region loses its identity page.

The diagnosis path is worth remembering: print `k_stacks`/`get_k_stack_top`/
`new_sp`/`trampoline_base` (all proved correct, ruling out the stack switch),
then read `phys_copy`'s caller from the nested-fault frame.  On amd64 the CPU
pushes RSP even for a same-privilege fault, so `frame->esp` is the faulting
`rsp`; `*(frame->esp)` is the (frameless) `phys_copy`'s return address — here
`apic_send_init_ipi+0x36`.

#### 9. `runqueues_ok_cpu` false-positives across CPUs *(panic right after CPU 1 is up)*

With #8 fixed, CPU 1 came up and servers started, then:

```
CPU 1 is up
sched error: ready proc 12 not on queue
proc.c:1666: assert "runqueues_ok_local()" failed, function "enqueue"
```

Not a real scheduling bug — a stock-MINIX debug check that was never SMP-correct,
live here only because this tree builds with `DEBUG_SANITYCHECKS=1` (it is `0` in
production, so the path is normally dead).  On SMP `runqueues_ok_local()` expands
to `runqueues_ok_cpu(cpuid)` (`proto.h`), which walks **only the checking CPU's**
per-CPU run queues (setting `p_found`), then flags *any* runnable process that
wasn't found.  A process assigned to a different CPU is legitimately on *that*
CPU's queue, so it trips the "ready proc N not on queue" check.  The base UP
branch never hits it because every process lives on cpu0's single queue.

Fix (`debug.c`): only flag a process that belongs to the CPU being verified:

```c
if(proc_is_runnable(xp) && xp->p_cpu == cpu && !xp->p_found) {
        printf("sched error: ready proc %d not on queue\n", xp->p_nr);
        return 0;
}
```

A genuinely misplaced process is still caught (by the pass for its own `p_cpu`);
on UP it is a no-op since every `p_cpu == 0`.  With #8 and #9, amd64 SMP boots to
`login:` with `cpunum=2`.

#### 10. `switch_to_user` asserts a runnable `proc_ptr` that another CPU just descheduled *(random reboot under load)*

With SMP booting to login, running the `minix-posix` test suite with `cpunum=2`
rebooted at a *random* test (4, 22, 25, 31, ... — different every run):

```
proc.c:357: assert "proc_is_runnable(p)" failed, function "switch_to_user"
kernel panic: assert failed
```

`switch_to_user` reads `p = proc_ptr`, and at the `check_misc_flags` label asserts
it is runnable:

```c
check_misc_flags:
	assert(p);
	assert(proc_is_runnable(p));	/* <- fired */
	while (p->p_misc_flags & (MF_KCALL_RESUME | MF_DELIVERMSG | ...)) {
#ifdef CONFIG_SMP
		if (!proc_is_runnable(p))
			goto not_runnable_pick_new;	/* loop already tolerates it */
#endif
```

The assert held on UP and at boot but failed only under true 2-CPU load.
Instrumentation (printf in the assert path — *never* in the `BKL_LOCK`/`BKL_UNLOCK`
macros; that hot path, including the `smp_schedule_sync` spin loops, deadlocks into
a silent hang) showed the stuck `proc_ptr` carried `RTS_PREEMPTED`, with `IF=0` and
the BKL held, and was **never** `proc_ptr` on two CPUs (so not a double-run).

Root cause: another CPU makes `proc_ptr` non-runnable between the moment it is
picked and the assert. `sched_proc` (`system.c`, with the upstream
`/* FIXME ... a problem for SMP if the process currently runs on a different CPU */`)
does a cross-CPU `RTS_SET`/`RTS_UNSET` on a process that is the running `proc_ptr`
on this CPU, and a scheduling IPI sets `RTS_PREEMPTED` on `proc_ptr`.  This is the
exact condition the misc-flags loop just below already tolerates with
`goto not_runnable_pick_new`; the entry assert simply predates SMP.

Fix (`proc.c`): recover the same way instead of asserting.  `not_runnable_pick_new`
clears `RTS_PREEMPTED` (the preempt-handling at the top of the path) and re-picks a
runnable process.

```c
check_misc_flags:
	assert(p);
#ifdef CONFIG_SMP
	if (!proc_is_runnable(p))
		goto not_runnable_pick_new;
#else
	assert(proc_is_runnable(p));
#endif
```

With #10 the suite runs past every prior crash point with only a few transient
recoveries per run.

#### 11. `sched_proc` cross-CPU re-quantum — *attempted fix REVERTED, do not retry naively*

It is tempting to "fix" the upstream `sched_proc` (`system.c`)
`/* FIXME ... problem for SMP */` by IPI-sync-stopping a process that runs on
another CPU before touching its RTS flags / quantum.  **Two such attempts were
made and both reverted** — record so the next person doesn't repeat them:

- *Always* `smp_schedule_stop_proc()` for a remote-running target → **BKL
  live-lock**: `SYS_SCHEDULE` fires constantly, two CPUs scheduling onto each
  other both enter `smp_schedule_sync` and ping-pong the BKL forever (both
  spinning in `arch_spinlock_lock`; caught via `virsh ... 'info registers -a'`).
- Skip the dequeue when `get_cpu_var(p->p_cpu, proc_ptr) == p` → **runqueues_ok
  panic** `wrong priority q N`: a process can be `proc_ptr` *and* still queued
  (the preempted-but-queued window in `switch_to_user`), so changing
  `p_priority` without dequeue leaves it on the wrong queue.

The upstream code is actually correct: it always dequeues (BKL-safe under the
big lock) before changing priority and only does the heavy IPI-stop for a real
migration.  `sched_proc` was restored to upstream verbatim.  test31's wedge was
**not** a `sched_proc` bug at all — it was the VFS mapped-inode bug (#12); with
#12 alone test31 is clean (errors and hang both gone).  Lesson: do not touch
this hot path without a reproducer that actually implicates it.

#### 12. VFS `put_vnode` leaves a dangling FIFO→PFS mapping *(test31 reopen corruption — the real test31 fix)*

A named FIFO's data is buffered in a *mapped* inode on PFS
(`v_mapfs_e`/`v_mapinode_nr`, created by `map_vnode()` in `servers/vfs/pipe.c`).
`put_vnode()` (`servers/vfs/vnode.c`) released that PFS inode (`req_putnode`) and
zeroed `v_mapfs_count`, but left `v_mapfs_e`/`v_mapinode_nr` pointing at the freed
inode — the reset only happened later in `get_free_vnode()` on reuse.  On UP the
slot is recycled before anyone looks; on SMP a concurrent reopen of the FIFO
(test31's second open/write/read cycle, parent and child on two CPUs) observes
the slot in that window and either skips remapping (`map_vnode()` early-returns on
`v_mapfs_e != NONE`, routing I/O to the freed PFS inode → `write` EINVAL / `read`
ENOENT) or putnodes the same PFS inode a second time (→ `VFS: putnode failed:
-22`).  Fix: clear `v_mapfs_e = NONE; v_mapinode_nr = 0;` in `put_vnode` right
after the mapped-FS `req_putnode`, the same reset `get_free_vnode`/`clean_vnode`
already do — done eagerly so the dangling mapping can't survive into the reopen.

#### 13. Kernel-to-user exit window ran with IF=1 → BKL leaked into user mode *(full-run deadlock / runqueue corruption)*

With #12, `test31` was clean but a **full** `minix-posix` run hung at a *random*
test (the suite would die ~test 4–31 every run, in three guises: a hard BKL
deadlock, a `runqueues_ok_local` "ready proc N not on queue" panic, or a reboot).
All three were the **same** root cause.

The leak: `switch_to_user()` (and the FPU `#NM` handler `copr_not_available_handler`)
ran the final kernel-to-user exit — `context_stop(proc_addr(KERNEL))` (which
`BKL_UNLOCK`s) → … → `restore_user_context()` — with **interrupts enabled**. A HW
interrupt taken in that window enters the in-kernel (`0:`) path → `context_stop_idle()`
→ `context_stop()` which **re-acquires the BKL**; the `iretq` (`CLEAR_IF`) returns
to the exit path now holding the BKL, and `restore_user_context()` carries it into
user mode. The next kernel entry on that CPU then self-deadlocks on `context_stop`'s
`BKL_LOCK` (recursive acquire), while the other CPU spins behind it. The same IF=1
window also let a scheduling IPI nest mid-`switch_to_user` and corrupt the run
queues (the `dequeue`/`runqueues_ok_local` panic).

Fix (`proc.c`): `intr_disable()` immediately before the final
`context_stop(proc_addr(KERNEL))` in both `switch_to_user()` and the FPU-exception
restore path, so the exit window runs IF=0 (the kernel invariant it should already
satisfy); `restore_user_context()`'s `iret`/`sysret` restores the user's own IF.
With this the suite runs **78 tests** (was ~3); the next failure (test 79) was a
PM signal-state assertion and an intermittent idle deadlock, both SMP-only — see
#14 and #15.

How it was found — the reusable SMP-hang toolkit:
- `virsh qemu-monitor-command <dom> --hmp 'info registers -a'` at the hang showed
  both vCPUs spinning in `arch_spinlock_lock` on `&big_kernel_lock`, IF=0, CPU
  time climbing (a *leaked* BKL: held with no live owner).
- A `BKL_DEBUG` build (temporary, since reverted) tracked the BKL owner cpuid +
  the **`__builtin_return_address(0)` of `BKL_LOCK`'s caller**, a per-CPU op ring
  (lock/unlock + caller), and dumped them over **raw COM1** (`direct_com_print`,
  which takes no BKL and survives the wedge) from a leaked-exit / recursive-acquire
  detector. The ring showed `… U switch_to_user(final release) … L context_stop_idle`
  — a BKL acquire *after* the final release — which pinned the exit-window nesting.
- **Never** `printf` from the `BKL_LOCK`/`BKL_UNLOCK` macros or `smp_schedule_sync`
  spin loops (silent deadlock); record into globals + raw-COM1 only.

#### 14. `do_runctl` cross-CPU stop races an in-transit send → PM `do_sigprocmask` assert *(test79)*

PM delivers a caught signal by stopping the target (`stop_proc` → `sys_delay_stop`
→ `SYS_RUNCTL` `RC_STOP|RC_DELAY`).  `do_runctl` decides whether the target has a
message *in transit* with a snapshot test (`RTS_SENDING` / `MF_SC_DEFER`): if so it
sets `MF_SIG_DELAY` and returns `EBUSY` (PM defers, waiting for `SIGSNDELAY`);
otherwise it stops the target and PM sets `PROC_STOPPED`.  On UP the snapshot is
exact (the target can't run while PM runs).  On SMP the target runs on another CPU
and can enter the kernel to `SENDREC` (e.g. `sigprocmask`) to PM *between* the
snapshot and the cross-CPU stop — `RTS_SENDING` isn't set yet, so `do_runctl`
returns OK, PM sets `PROC_STOPPED`, and the in-transit message then arrives →
`pm/signal.c do_sigprocmask` asserts `!(mp_flags & (PROC_STOPPED|VFS_CALL|UNPAUSED|
EVENT_CALL))` and PM panics (which RS cannot recover → kernel `cause_sig` lethal
panic).

Key: `smp_schedule_stop_proc()` is **synchronous** — it returns only after this CPU
re-acquires the BKL, by which point the target CPU has finished delivering its
message and is blocked `SENDING`.  So re-check after the stop:

```c
case RC_STOP:
#if CONFIG_SMP
    if (rp->p_cpu != cpuid) {
        smp_schedule_stop_proc(rp);
        /* the snapshot above may have missed a send the target (running on
         * another CPU) only issued during the stop; it is now SENDING. */
        if ((flags & RC_DELAY) &&
            (RTS_ISSET(rp, RTS_SENDING) || (rp->p_misc_flags & MF_SC_DEFER))) {
            rp->p_misc_flags |= MF_SIG_DELAY;
            RTS_UNSET(rp, RTS_PROC_STOP);   /* mandatory — see below */
            return (EBUSY);
        }
        break;
    }
#endif
    RTS_SET(rp, RTS_PROC_STOP);
```

`RTS_UNSET(RTS_PROC_STOP)` is mandatory: PM treats `EBUSY` as a delay call
(`DELAY_CALL`, not `PROC_STOPPED`) and never issues the matching resume, so leaving
the kernel stop set strands the process (both CPUs idle).  Clearing it is safe — the
process stays blocked `SENDING` (not runnable here); the normal send-completion path
reschedules it and fires `SIGSNDELAY`, after which PM retries the stop.  This mirrors
the UP up-front `EBUSY` path exactly.  (commit b6b9b7b55)

#### 15. `idle()` setup-and-halt window ran with IF=1 → lost wake-IPI on an AP *(test79 intermittent whole-system idle hang)*

The **same bug class as #13**, in the other IF-sensitive window.  `idle()` set
`cpu_is_idle = 1`, stopped the AP's local timer, and `halt_cpu()`d — all with
interrupts enabled.  A HW interrupt/IPI taken between `cpu_is_idle = 1` and the `hlt`
enters the in-kernel `context_stop_idle()`, which **resets `cpu_is_idle = 0`** (and
restarts the AP timer that `idle()` then re-stops).  `idle()` falls through to
`halt_cpu()` and the CPU halts with `cpu_is_idle == 0` and, on an AP, its local timer
stopped.  `enqueue()` on another CPU then reads `cpu_is_idle == 0`, concludes the
target is running, and **skips the wake IPI** (`smp_schedule`); since the AP has no
timer tick, a runnable process sits on the halted AP's run queue forever → all CPUs
idle, system wedged.

```c
switch_address_space_idle();

intr_disable();   /* NEW: keep the whole window IF=0; halt_cpu() does sti;hlt */

#ifdef CONFIG_SMP
    get_cpulocal_var(cpu_is_idle) = 1;
    if (cpuid != bsp_cpu_id)
        stop_local_timer();     /* AP: only an IPI can wake it after this */
    else
#endif
        restart_local_timer();
    context_stop(proc_addr(KERNEL));   /* BKL_UNLOCK */
    halt_cpu();                 /* sti; hlt — re-enables and atomically catches
                                 * an IPI that went pending while IF=0 */
```

The BSP **masks** the symptom (it keeps its timer and re-checks `pick_proc()` every
tick), so only APs (which stop their timer when idle) deadlock — which is why the
stranded procs were always on `cpu != bsp`.

How it was found: the `info registers -a` toolkit from #13 showed both vCPUs `HLT`ed
(*idle*, not spinning on the BKL).  A temporary deadlock probe in the BSP `idle()`
path dumped the run state — `nrun=4` processes runnable on `cpu=1` with `onq=1` (on
the AP's run queue) and `cpuidle=0` (the AP's flag wrong while halted) — which pinned
the IF window in `idle()`.  General lesson: any kernel window that sets per-CPU
idle/exit state and then halts or returns to user must run IF=0; the BSP-timer +
wake-IPI recovery does **not** cover an AP that has stopped its own timer.  (commit
28119c23a)

#### 16. `mini_receive` delivered a dead sender off the caller queue → assert *(test79, intermittent)*

With #14 and #15, test79 ran to completion but **intermittently** panicked in a
later sub-test:

```
proc.c: assert "!RTS_ISSET(sender, RTS_SLOT_FREE)" failed, function "mini_receive"
kernel on CPU 0: … do_ipc … ipc_entry_common
```

`mini_receive` walks the receiver's `p_caller_q` and asserts each queued sender is
live — a uniprocessor invariant: there, a process being torn down can't also be
sitting on a send queue.  On SMP it can.  A process queued to send (`RTS_SENDING`,
linked on the target's `p_caller_q`) is torn down on another CPU — `do_clear()` →
`clear_endpoint()` → `clear_ipc()`/`clear_ipc_refs()` unlink it and then
`RTS_SETFLAGS(rc, RTS_SLOT_FREE)` — and that teardown can race the target's
`mini_receive` queue walk, leaving a `SLOT_FREE` (or `RTS_NO_ENDPOINT`) entry still
linked.  Delivering it trips the assert and panics the kernel.  It was surfaced by
test79's heavy fork/exit/signal load, amplified by an `e1000`/`virtio_net` restart
loop churning process exits (`IRQ 22 handler registered by … /<ep>` with climbing
endpoint generations).

Fix (`proc.c mini_receive`): before the `CANRECEIVE` test, if the queued sender is
`RTS_SLOT_FREE` or `RTS_NO_ENDPOINT` it is a dead message whose sender will never
wait for a reply — **unlink and skip it** instead of delivering/asserting (with a
rate-limited warning, since it is a genuine but handled SMP anomaly).

```c
while (*xpp) {
    struct proc *sender = *xpp;
    if (RTS_ISSET(sender, RTS_SLOT_FREE) || RTS_ISSET(sender, RTS_NO_ENDPOINT)) {
        *xpp = sender->p_q_link;      /* drop stale entry */
        sender->p_q_link = NULL;
        continue;
    }
    ...
}
```

(commit 96fe5bfec)

With #14–#16, **test79 passes** (`ok`) and the full `minix-posix` suite runs past it.
*Open, separate:* the `e1000`/`virtio_net` IRQ-22 restart loop under test79's load is
pre-existing and non-fatal, but it is the exit churn that triggers #16; root-causing
that driver/RS crash loop is future work.

### #17 — AP local timer never fires for short-burst processes (test41)

test41 (`getitimer`/`setitimer`) failed only on `ITIMER_VIRTUAL`/`ITIMER_PROF`
(REAL passed); the virtual/profiling signal was never delivered.  With `no_smp=1`
it passed, so it was SMP-specific.

Root cause: the **AP local APIC timer never delivered a single interrupt**.
The scheduler (`sched` `pick_cpu`) places user processes on APs while system
services (incl. PM) stay on the BSP, so the test process runs on an AP — but a
LAPIC timer that never ticks there means `p_user_time`/`p_virt_left`/`p_prof_left`
are never charged, so `vtimer_check` never fires `cause_sig(SIGVTALRM/SIGPROF)`.
REAL works because it is driven entirely by the BSP (`kclockinfo.uptime` +
`clock_timers`).  A per-CPU tick probe in `timer_int_handler` confirmed only
`cpu=0` ever ticked.

The LAPIC timer is one-shot, re-armed in `switch_to_user`/`context_stop_idle`
(`lapic_restart_timer`, which re-arms only when `CCR==0`).  `idle()` stops the
timer on APs (`stop_local_timer`) — correctly, because a *halted* AP must not take
a local timer interrupt: the in-kernel interrupt path runs `context_stop()`, which
spins on the BKL with interrupts disabled (interrupt-gate entry) and can deadlock
against the BSP holding the BKL while waiting for an IPI ack from that AP.  (An
early fix that simply kept the AP timer running confirmed this — it made the AP
tick but **hung the boot** in a BKL deadlock.)

The real defect: `lapic_stop_timer` *zeroed* the one-shot ICR, and the next
`lapic_restart_timer` armed a **full fresh period** from the top.  A process that
yields (blocks in a syscall) more often than once per tick — e.g. test41's
`busy_wait` calling `time()` every few µs — makes its AP idle every few µs; a
register dump showed the one-shot perpetually re-armed at ICR=16,666,000 and only
ever counted down ~13,000 (0.08%) before the next idle reset it.  It never reached
0, so the AP never ticked.

Fix (`apic.c`): preserve the remaining count across idle.  `lapic_stop_timer`
saves `CCR` into `lapic_timer_rem[cpu]`; `lapic_restart_timer` resumes that saved
count (writes it back to ICR) instead of arming a fresh period.  An AP that idles
often now accumulates *run-time* toward a tick across bursts; the one-shot
eventually expires while a process runs → AP tick → `p_virt_left` decrements →
`SIGVTALRM`/`SIGPROF` fire.  Idle APs still stop their timer (no BKL deadlock).

```c
/* lapic_stop_timer: before masking + zeroing ICR */
lapic_timer_rem[cpuid] = lapic_read(LAPIC_TIMER_CCR);

/* lapic_restart_timer: resume the saved partial countdown */
u32_t rem = lapic_timer_rem[cpu];
if (rem) {
    lapic_timer_rem[cpu] = 0;
    lapic_write(LAPIC_TIMER_ICR, rem);
    lapic_write(LAPIC_TIMER_DCR, APIC_TDCR_1);
    lapic_write(LAPIC_LVTTR, APIC_TIMER_INT_VECTOR);
    return;
}
if (lapic_read(LAPIC_TIMER_CCR) == 0)   /* else: normal full re-arm on expiry */
    lapic_set_timer_one_shot(1000000/system_hz);
```

`timer_int_handler` still updates wall-clock time (`uptime`) only on the BSP, so
APs add no timekeeping cost — only the per-CPU accounting tick they need.  With
this, **test41 passes** on SMP (verified end-to-end: AP `dec` → `vtimer_check` →
PM `process_ksig` → signal delivered).  How it was found: per-CPU tick counter +
LAPIC register dump in `lapic_restart_timer` showed the never-completing one-shot.

**i386 applies** — `arch/i386/apic.c`'s `lapic_stop_timer`/`lapic_restart_timer`
were byte-for-byte the pre-fix amd64 versions (LAPIC timer registers are
identical), so the same `lapic_timer_rem[]` save/resume was ported.  Latent until
i386 SMP actually schedules user procs onto an AP.

### #18 — IOAPIC re-fires an auto-re-enabled IRQ whose device line is still asserted → interrupt storm *(SMP boot hang on VirtualBox)*

SMP (i386 **and** amd64) hung on VirtualBox during driver bring-up; the same tree
booted fine **non-SMP** on VBox and **SMP on QEMU**.  The console froze after `rs
server: started`, and a `switch_to_user` probe showed RS being re-picked millions
of times, its user pc **frozen** at the instruction right after its `SENDA`
`syscall` (the SYSRET return point) — executing *zero* userland instructions.
That framing ("RS spins on SENDA") was a red herring: RS is simply the
highest-priority runnable process, and something re-entered the kernel the instant
`restore_user_context` re-enabled interrupts, every time, so RS never advanced.

Not a syscall (`do_ipc` entered once), not a fault (`exception_handler` fired once
— a single legitimate demand-page on RS's stack, resolved by VM), and **not the
timer** (`arch_timer_int_handler` ticked once; period was a normal ~16 M).  A
per-IRQ counter in `irq_handle` found it: one device IRQ flooding in lockstep with
`switch_to_user`.  As each fix landed, the storm **walked to the next driver**:
IRQ 12 (PS/2 mouse) → IRQ 1 (keyboard) → IRQ 14 (at_wini disk).  The tell: floppy
(IRQ 6) registered but never stormed — it does **not** use `IRQ_REENABLE`.

Root cause is generic, in the `IRQ_REENABLE` + IOAPIC interaction.  `irq_handle`
masks the IOAPIC pin, calls the handler (which notifies the userspace driver), and
for an `IRQ_REENABLE` line **immediately unmasks the pin again** — before the
driver process has been scheduled to read/drain the device.  Devices like the 8042
(keyboard/mouse) and legacy IDE hold their IRQ line **asserted** (8042 `OBF`=1 /
IDE status pending) until the driver reads the data port.  VBox's emulated IOAPIC
re-fires an edge pin that is unmasked while its input is still asserted → the IRQ
re-delivers instantly → the storm monopolizes the CPU so the driver is *never*
scheduled to drain it → self-sustaining livelock.  The legacy **8259 PIC**
(non-SMP path) does not re-fire an asserted edge line on unmask, which is exactly
why non-SMP boots.  MINIX programs ISA IRQs edge/active-high correctly
(`set_irq_redir_low`, irq<16); the re-fire is VBox re-asserting the held-high pin.

There is no clean *kernel* fix that keeps `IRQ_REENABLE`: auto-unmask-before-drain
is inherent to the policy, and suppressing the IOAPIC re-fire is VBox-behavior
specific and conflicts with the drain-first model.  Fix is per-driver
**drain-before-reenable**: use policy `0` (not `IRQ_REENABLE`) so the line stays
masked after each interrupt, and have the driver call `sys_irqenable()` **after**
it reads the device (which drops the line), so the pin is unmasked only when its
input is already low.

```c
/* pckbd (drivers/hid/pckbd/pckbd.c) */
-   r = sys_irqsetpolicy(KEYBOARD_IRQ, IRQ_REENABLE, &irq_hook_id);
+   r = sys_irqsetpolicy(KEYBOARD_IRQ, 0, &irq_hook_id);   /* stay masked */
    ...
/* pckbd_intr(): after scan_keyboard() reads port 0x60 (clears OBF) */
+   (void) sys_irqenable(&irq_hook_id);   /* re-enable only after draining */
    /* (the PS/2 AUX/mouse port is also left disabled: aux_available = 0) */

/* at_wini (drivers/storage/at_wini/at_wini.c) — legacy IRQ 14/15 only */
-   sys_irqsetpolicy(AT_WINI_0_IRQ, IRQ_REENABLE, &compat_hook);
+   sys_irqsetpolicy(AT_WINI_0_IRQ, 0, &compat_hook);
    ...
/* w_intr_wait(), HARDWARE case, after the REG_STATUS read that acks the drive */
+   if (!w_wn->native)
+       (void) sys_irqenable(&w_wn->irq_hook_id);
```

With pckbd + at_wini fixed, i386 SMP **boots to `login:`** on VBox.  These live in
driver files shared with amd64, so they also clear the amd64-on-VBox hang.
`rs232` (tty serial) was later converted to the same policy `0` pattern to fix the
VBox COM2 storm (commit a633fcad7).  Still **latent**: `dpeth` and `vbox` also use
`IRQ_REENABLE`; they only storm if their line happens to be asserted at boot before
their driver runs.  Any future "boots to a point then wedges on IRQ N" under APIC is
the same bug — apply the same drain-then-`sys_irqenable` pattern to driver N.

**But the naive drain-before-reenable pattern regresses QEMU — see #19.**
(See also the pointer in `docs/drivers-x86_64.md`.)

### #19 — the drain-before-reenable fix (#18) loses interrupts on a *correct* IOAPIC → dead keyboard/serial *(QEMU)*

The #18 fix (policy `0` + `sys_irqenable()` after draining) shipped for pckbd /
at_wini / rs232 and booted VBox to `login:` — but then **QEMU's keyboard went
dead**: the amd64 SMP tree booted all the way to `login:` (see a clean run in
`log/test-1.log`) yet accepted **no key input**.  The #18 fix, which cured VBox,
*created* a QEMU bug — the two hypervisors fail in opposite directions.

Mechanism.  With policy `0`, `irq_handle` masks the IOAPIC pin and — because
`generic_handler` returns `hook->policy & IRQ_REENABLE == 0` — leaves the actids
bit set, so the pin stays **masked** from the interrupt until the driver calls
`sys_irqenable()`.  During that masked window the device can raise its line again
(a second key byte, a serial RX char).  On a **correct edge-triggered IOAPIC
(QEMU/real hardware)** an edge that arrives while the pin is masked is **lost** —
edge pins have no remote-IRR latch.  When the driver finally unmasks, the line is
still asserted (8042 `OBF`=1 / UART `INTR` high) but there is **no fresh edge**, so
the interrupt never re-delivers; the device stops producing new edges (OBF blocks
further bytes) → the input path wedges **permanently**.  VBox never hit this
precisely because its IOAPIC *re-fires* a still-asserted line on unmask (the very
misbehavior that caused the #18 storm) — which accidentally recovers the lost edge.
So VBox's bug masked QEMU's.

Two distinct defects had to be fixed:

1. **pckbd read only one byte per interrupt.**  `scan_keyboard()` reads a single
   byte from port 0x60; a second byte queued (or arriving in the masked window)
   left `OBF`=1 with no future edge → dead keyboard.  Fix: drain in a **loop**
   until the 8042 output buffer is empty, then close the masked-window race by
   re-checking `KB_STATUS` **after** unmasking and looping if a byte slipped in.

   ```c
   /* pckbd_intr() — drivers/hid/pckbd/pckbd.c */
   do {
       while (scan_keyboard(&scode, &isaux)) { /* drain ALL queued bytes */
           if (!isaux) kbd_process(scode); else kbdaux_process(scode);
       }
       (void) sys_irqenable(&irq_hook_id);         /* unmask (line now low) */
       if (sys_inb(KB_STATUS, &sb) != OK) break;   /* did a byte race in? */
   } while (sb & KB_OUT_FULL);                      /* its edge was lost — redo */
   ```

2. **rs232 only needed the race-closer.**  `rs232_handler()` already loops on IIR
   until `IS_NOTPENDING` (full drain), so only the masked-window edge could be
   lost.  Fix: after `sys_irqenable()` re-read the IIR (`int_id_port`); if an
   interrupt is pending, drain + re-arm again (retry-bounded so a stuck-asserting
   UART can't spin).  Applied to both arch copies.

`at_wini` needed a **different** fix (no race — IDE is request/response, so it
raises no interrupt while masked).  Its gap: `w_hw_int()` (the blockdriver
`.bdr_intr` leftover-interrupt handler) acked + re-enabled **native** drives only;
under policy `0` a leftover interrupt on a **compat** drive reaches `w_hw_int()`
directly (not `w_intr_wait()`), so its IRQ 14/15 stayed masked forever.  Fix:
`w_hw_int()` now reads `REG_STATUS` (ack) and `sys_irqenable()`s **all**
non-`IGNORING` drives (the DMA-status dance stays native-only), and the redundant
inline compat re-enable in `w_intr_wait()` was removed.

Rule of thumb: any policy-`0` drain-before-reenable driver on a real
edge-triggered IOAPIC must **drain the device fully** (loop, not one unit) *and*
**re-check the device after unmask** to recover an edge lost in the masked window.
Confirmed: amd64 SMP keyboard input works on QEMU with these fixes, VBox still boots.

### #20 — LAPIC EOI faults on an AP because its mapping lives in the (per-process) user half *(SMP test42 reboot)*

`cpu=4`, POSIX **test42 (ptrace)** rebooted:

```
Test 42 pagefault in kernel at pc 0xffffffff804cf59a address 0xf00060b0
cpu 1 is_nested = 1 vec_nr=14 trap_errno=0x2   (write, page not-present)
```

`0x804cf59a` is the EOI store in `lapic_timer_int_handler`
(`movq lapic_eoi_addr(%rip),%rax; movl $0,(%rax)`); `%rax = 0xf00060b0` = LAPIC
`+0xb0` (EOI register). `0xf0006000` is a **user-half** address: on x86_64 VM maps
the kernel device windows (LAPIC/IOAPIC/video) at `VM_DATATOP` (`user_end`) inside
**PML4[0]** so the `usermapped` region stays user-reachable (see
`servers/vm/pagetable.c`). But the LAPIC EOI is written by the kernel on **every
timer tick on every CPU**, in whatever process address space is current at the
time — and a user-half mapping is subject to per-process page-table churn. Under
test42's heavy `SYSTEM`-task (endpoint `-2`) activity a tick landed on an AP whose
current CR3 lacked the LAPIC page → not-present kernel write → the nested-fault
handler re-faulted (`memcpy`, addr `0x8`) → triple-fault reboot.

Fix: reach the LAPIC/IOAPIC through the kernel **physical direct map** instead.
`pg_identity()` already maps the low ≥4 GB (incl. LAPIC `0xFEE00000` / IOAPIC
`0xFEC00000`) at `PHYS_DIRECTMAP_BASE` under the shared **PML4[511]**
(`pg_pdpt_high`/`pg_pd_dm`, wired into every address space via `kern_pml4_hi`).
`phys_to_kacc(pa) = PHYS_DIRECTMAP_BASE + pa` is that alias — kernel-only and
unconditionally present. In `arch_enable_paging()`:

```diff
 #ifdef USE_APIC
     if (lapic_addr) {
-        lapic_addr = lapic_addr_vaddr;      /* VM user-half vaddr (PML4[0]) */
+        lapic_addr = phys_to_kacc(LOCAL_APIC_DEF_ADDR);   /* direct map, PML4[511] */
         lapic_eoi_addr = LAPIC_EOI;
     }
     if (ioapic_enabled)
         for (i = 0; i < nioapics; i++)
-            io_apic[i].addr = io_apic[i].vaddr;
+            io_apic[i].addr = phys_to_kacc(io_apic[i].paddr);
 #endif
```

`pg_identity()` gained a `PG_APIC_MMIO(phys)` guard (`[0xFEC00000,0xFF000000)`)
forcing `PG_PWT|PG_PCD` on those direct-map/identity leaves, so the alias is
uncached even when RAM extends past the 4 GB hole (`mem_high_phys > 4 GB`).

**Idempotency gotcha (cost a boot cycle).** `arch_enable_paging()` runs on **every**
`VMCTL_ENABLE_PAGING` (`arch_do_vmctl.c`), not once. A first attempt used the
in-place form `lapic_addr = phys_to_kacc(lapic_addr)`; the second call re-aliased
an already-aliased value:

```
0xFFFFFF8000000000 + 0xFFFFFF80FEE00000 = 0x1FFFFFF00FEE00000
                          → truncates to  0xFFFFFF00FEE00000   (bit 39 gone → PML4[510], unmapped)
```

which faulted in `lapic_restart_timer` at `+0x390` (`LAPIC_TIMER_CCR`) during init
(`address 0xffffff00fee00390`). Always derive the alias from the **fixed physical
base** (`LOCAL_APIC_DEF_ADDR`), never transform `lapic_addr` in place — the old
`lapic_addr = lapic_addr_vaddr` was idempotent, which hid the multiple-call path.
`io_apic[].addr` reads `.paddr` (stable phys), so it is idempotent as written.

Result: the suite runs clean through test84, stopping at the known non-arch-specific
`test85` vnd deadlock (below). VM still creates the now-unused user-half LAPIC/IOAPIC
mapping (`lapic_addr_vaddr`, `u32_t`, is dead) — harmless, a follow-up cleanup.

### #21 — `runqueues_ok_cpu` false-positives during an in-flight cross-CPU sync *(test79 whole-suite reboot)*

A full POSIX run **rebooted at test79** (PM signal stress):

```
Test 79 sched error: ready proc 0 not on queue
proc.c:1747: assert "runqueues_ok_local()" failed, function "enqueue"
kernel panic: assert failed
kernel on CPU 0: ... enqueue ... mini_send ... ipc_entry_common
```

`ready proc 0` = **PM** (`PM_PROC_NR`): PM was found runnable (`p_rts_flags == 0`) with
`p_cpu == 0` but not on any of CPU 0's run queues, during the `DEBUG_SANITYCHECKS` walk
at the tail of `enqueue`. Running `test79` in isolation instead **hangs** — same churn,
the intermittent assert just does not always land.

Two independent SMP characteristics were tangled here; only the second is a bug:

1. **Not a bug — inherent slowness.** Instrumenting the hang (per-CPU clock heartbeat +
   per-`SYS_*`-call rates) showed the machine fully alive: clock ticking, ~20k
   dispatches/s, dominated by `RUNCTL` (~35k/interval) + `SIGSEND`/`SIGRET` (~16k each) —
   the PM cross-CPU signal-delivery handshake, *progressing* (fork-child endpoints
   advance, `SIGSEND ≡ SIGRET`, no `e(0)` desync abort). Cutting `NR_SIGNALS` 20000→200
   made test79 finish (`ok`) in ~40 s; ×100 for the real count ≈ 65 min, which reads as a
   hang. Root cause is the **big kernel lock**: test79 is kernel/IPC-bound, only one CPU
   is in the kernel at a time, so 2 CPUs get no parallelism yet pay cross-CPU wake-IPI +
   BKL-handoff overhead → *slower* than UP (which passes). A per-signal micro-opt in
   `smp_schedule_stop_proc` (skip the stop-IPI when the target is queued-not-running) was
   tried and **measured to do nothing** — the stops were already cheap (target almost
   always blocked, no IPI); the IPI was never the bottleneck. Reverted. Accepted as an
   architectural BKL limitation, not chased further.

2. **The bug — the sanity check is not SMP-race-safe.** `runqueues_ok_local()` =
   `runqueues_ok_cpu(cpuid)` scans the *whole* proc table for procs that are runnable,
   `p_cpu == cpuid`, and not on a queue. That invariant holds under the BKL — **except**
   inside `smp_schedule_sync()`, which `BKL_UNLOCK()`s and spin-waits for the target CPU
   to ack a STOP/VMINHIBIT/SAVE_CTX/migrate (`smp.c`). During that window another CPU
   enters the kernel, does an `enqueue`, and runs the check while the run queues are
   legitimately mid-transition — PM momentarily runnable yet not (yet) re-queued. Same
   class as the `p_cpu == cpu` filter added in #9; that filter only covers procs owned by
   *another* CPU, not this transient. (Reasoning could not statically prove the exact
   interleaving, so the fix was shipped with a `dump_scheddump()` net on the failure path;
   the full-suite run then reached the last test with the assert gone.)

Fix: skip the whole-table "not on queue" flag while any cross-CPU sync is in flight.
`smp.c` exposes the predicate; `debug.c` consults it (the queue-*integrity* checks above
it are local-CPU and stay):

```c
/* smp.c */
int smp_sched_ipi_pending(void)
{
	unsigned c;
	for (c = 0; c < ncpus; c++)
		if (sched_ipi_data[c].flags != 0)
			return 1;
	return 0;
}
```
```diff
  /* debug.c runqueues_ok_cpu(), whole-table scan */
  if(proc_is_runnable(xp) && xp->p_cpu == cpu && !xp->p_found) {
+#ifdef CONFIG_SMP
+     /* smp_schedule_sync() dropped the BKL and is spin-waiting for an ack;
+      * the run queues are transiently inconsistent as seen from this CPU.
+      * A proc off-queue for any OTHER reason still has no sync pending. */
+     if (smp_sched_ipi_pending())
+         continue;
+#endif
      printf("sched error: ready proc %d not on queue\n", xp->p_nr);
      return 0;
  }
```

`DEBUG_SANITYCHECKS` is on in this tree (`debug.h`), so this check is live; a production
build (checks off) never panicked, it only ever ran slow. With the guard the **full POSIX
suite reaches the last test on SMP**; test79 is still slow (BKL, above) but no longer
aborts the run. See CLAUDE.md ("`runqueues_ok_cpu` … must filter by `xp->p_cpu == cpu`")
and #9/#10 for the earlier instances of this same not-SMP-safe-check theme.

## EFI64 boot bring-up — first successful boot (2026-06)

The UEFI/multiboot2 path had never run to completion; fixing it surfaced one
fault at a time from the loader hand-off through `arch_init`.  In order:

1. **Multiboot2 header out of search window** — `ld` defaulted to a 2 MB
   max-page-size, pushing the first `PT_LOAD` (and the header in
   `.unpaged_text`) to file offset `0x200000`, past the loader's 32 KB
   `MULTIBOOT_SEARCH`.  Fix: link the kernel with `-Wl,-z,max-page-size=0x1000`
   (`minix/kernel/Makefile`).  See `docs/kernel-build-x86_64-fixes.md`.

2. **NULL trampoline in the loader** — `efi_md_init()` (copies the
   `multiboot64`/`startprog64` trampolines into allocated memory and sets the
   function pointers) was never called from `efi_main()` in
   `sys/stand/efiboot/efiboot.c`.  `multiboot64` stayed at its `.quad 0`
   initializer, so `(*multiboot64)()` jumped through NULL to address 0.  Fix:
   call `efi_md_init()` before `boot()`.  (Latent because the multiboot2 path
   never reached the jump before.)

3. **EFI64 entry emitted into `.unpaged_data`** — in `head.S` the `.code64`
   directive does *not* switch sections back after the preceding `.data`
   block, so `multiboot_entry64_efi` landed in `.unpaged_data`.  It happened to
   work (boot map is RWX) but is wrong; add an explicit `.text` before the
   entry.

4. **Boot identity map too small (16 MB)** — `multiboot_entry64_efi`'s boot
   page tables identity-mapped only 0–16 MB, but the EFI loader places the
   multiboot info struct (via `AllocateAnyPages`) well above that (~2 GB on
   OVMF).  `pre_init()` dereferences it in `get_parameters_mb2()` before
   building the full map → triple fault.  Fix: identity-map the low **4 GB** in
   the boot tables (4 PD pages, `boot_pd_low` enlarged to 4 pages).

5. **`lldt` #GP in `prot_init`** — on x86_64 an LDT descriptor is a 16-byte
   system descriptor, but the code built an 8-byte one at `gdt[LDT_INDEX]`, so
   `lldt LDT_SELECTOR` faulted (#GP, error = the LDT selector `0x28`).  x86_64
   uses a flat GDT with no per-process LDT, so the fix is `x86_lldt(0)` (null
   LDTR) and dropping the bogus descriptor (`protect.c`).  The TSS descriptor
   is already correctly built as 16 bytes (`tss_init`).

6. **ACPI XSDT alignment padding** — see `docs/apic-x86_64.md`.

7. **LAPIC MMIO unmapped after `prot_init`** — see `docs/apic-x86_64.md`.

The boot now reaches APIC initialization.

## EFI64 bring-up — reaching userland exec (2026-06)

Continuing from APIC init, the boot was carried through VM adopting the
per-process page tables and into userland: `exec` works and `init` runs
`/etc/rc`, starting drivers (acpi, pci, pckbd, floppy, at_wini) and services.
Two themes dominated.

### Theme 1: 64-bit addresses truncated through 32-bit IPC message fields

A user virtual address with bit 31 set (e.g. a stack address `0xefffXXXX`,
since the MINIX user stack top is `0xF0000000`) stored into a 32-bit `int`
message field and read back into a `vir_bytes` is **sign-extended into the
kernel half** (`0xffffffffefffXXXX`).  The receiver then rejects it as a
kernel address.  Three instances, all in `minix/include/minix/com.h`
(+`ipc.h`):

1. **`VPF_ADDR`** (`VM_PAGEFAULT` fault address, kernel→VM): `m1_i1` →
   `m1_ull1`.  Symptom: a normal demand-paged stack-growth fault (`cr2` was
   correct) became a kernel-half address in VM, which SIGSEGV'd the faulting
   server.
2. **`SVMCTL_MRG_ADDR`** (kernel→VM memory-request address, the `vm_suspend`
   path): `m2_i2` → `m2_ll1`.  Tell-tale: the *source* address
   `SVMCTL_MRG_ADDR2` was already a 64-bit `m2_l2`; only the primary address
   was left 32-bit.
3. **`VFS_PM_PS_STR` / `VFS_PM_NEWPS_STR`** (`ps_strings` pointer, VFS↔PM exec
   reply): `m7_i5` → a new pointer field **`m7_p3`** added to `mess_7`
   (`ipc.h`, consuming padding; i386 size stays 56 bytes via alignment).  Note
   `VFS_PM_PC`/`VFS_PM_NEWSP` already used pointer fields (`m7_p1`/`m7_p2`), so
   `ip`/`sp` survived while `ps_str` was truncated.

This pattern is **systemic**: audit any `m*_i*` field carrying an address.
Downstream typed structs (`mess_rs_pm_exec_restart`, `mess_lsys_krn_sys_exec`)
already use `vir_bytes ps_str`.

### Theme 2: the demand-paged kernel copy (the exec stack frame)

The single biggest blocker.  `lin_lin_copy()` (`arch/x86_64/memory.c`) copies
between address spaces using `createpde()`, which returns **0** for a
not-present process page (e.g. a child's demand-zero stack during the exec
frame copy).  The old code relied on `PHYS_COPY_CATCH` to detect a bad page,
but with `dstptr == 0` the copy touches **virtual address 0**, whose caught
fault address is `0` — *indistinguishable from the `if(addr)` "no fault"
sentinel*.  So the copy silently "succeeded" writing nowhere, VM was never
asked to fault the page in, and every later access got a fresh zero page.
Text/data escaped this only because the ELF loader pre-allocates them (present
→ real phys).  The user-visible effect: the first user process (`sh`) found
`ps_strings->ps_argvstr == 0` and `#PF`'d in crt0.

Fix — detect not-present explicitly so `virtual_copy_f()` routes through
`vm_suspend()`:

```c
srcptr = createpde(srcproc, srclinaddr, &chunk, 0, &changed);
dstptr = createpde(dstproc, dstlinaddr, &chunk, 1, &changed);
if (srcproc && !srcptr) return EFAULT_SRC;   /* not-present -> ask VM */
if (dstproc && !dstptr) return EFAULT_DST;
```

(`createpde` returns `phys_to_kacc(phys)` = `DM_BASE + phys` for present
pages, never 0, so `!ptr` cleanly means not-present.)

### Other fixes folded in

- **`pt_free()` freed `pt_pdpt` but left the pointer non-NULL**
  (`servers/vm/pagetable.c`).  `pt_new()` allocates the PML4+PDPT once and
  reuses them (recovering the PDPT phys from the kept `pt_pml4[0]`); the stale
  non-NULL `pt_pdpt` made the next `pt_new()` `memset` a freed/unmapped page →
  "pagefault in VM" on `VMPPARAM_CLEAR` slot reuse (exec).  Fix: keep
  `pt_pdpt` (never free, same policy as `pt_pml4`) and NULL the freed
  `pt_pd[]`/`pt_pt[]` slots.

- **`SYS_DEVIO`/`VDEVIO`/`SDEVIO`/`IOPENABLE`/`READBIOS` were i386-only**
  (`kernel/system.c` `map()` calls + `system/Makefile.inc` `do_devio.c`/
  `do_vdevio.c` sources).  amd64 has identical x86 port I/O; gate them
  `__i386__ || __x86_64__`.  Symptom: "Unused kernel call 21" and tty/driver
  `sys_inb`/`sys_outb` failures.

- **`arch_proc_init()` truncated `ip`/`sp`/`ps_str` to `u32_t`**
  (`do_exec.c` casts + `proto.h` + all three `arch/*/memory.c`): widen to
  `vir_bytes`.  Latent for sh (values < 4 GB) but a real LP64 bug.

- **VFS computes `ps_str` authoritatively** (`servers/vfs/exec.c` `pm_exec`):
  the `ps_strings` struct always sits at the top of the (possibly relocated)
  frame, so `*ps_str = vsp + frame_len - sizeof(struct ps_strings)` rather
  than echoing the caller's guess (robust against stale-libc callers and
  script/dynamic frame relocation).

- **libc stack frame** (`stack_utils.c`, `execve.c`): `vsp` widened `int` →
  `vir_bytes`; argc occupies a pointer-sized cell so `argv[]` starts
  `sizeof(char *)` in, and `ps_argvstr = vsp + sizeof(char *)`.

## Physical memory map (multiboot2) — usable-RAM fixes

`get_parameters_mb2()` builds `cbi->memmap` (the kernel's free-RAM list, later
fed to VM) from the multiboot2 mmap tag.  Two fixes were needed before VM could
be trusted with the memory it hands out — both caused VM to place structures on
**non-RAM physical pages**, whose writes vanish, producing pervasive corruption
(details in `vm-x86_64-port.md`):

- **`do_tag_mmap()` truncated 64-bit addresses.**  `base_addr`/`length` were
  `uint32_t`, truncating the firmware `mmap->addr`/`len` (folding a >4 GB region
  down to a bogus low address that overlaps real low RAM).  Use `u64_t`.

- **Legacy ROM hole left in usable RAM.**  Firmware reports `0xA0000–0x100000`
  (VGA framebuffer + option/BIOS-ROM shadow) as available, but under QEMU those
  pages are ROM/MMIO and silently drop writes.  `get_parameters_mb2()` already
  carves out the kernel and boot modules with `cut_memmap()`; it now also does
  `cut_memmap(cbi, 0xA0000, 0x100000)`.  Symptom before the fix: kernel
  `vm_memset` zeroing a page at `ph=0xff000` left it `0xffffffff` (the
  read-back-verify that found it).

With these (plus the VM `findbit()` LP64 fix), MINIX/amd64 boots to a multiuser
login prompt and working root shell.  (The earlier "no login prompt" stall was a
symptom of this corruption, not a separate console bug.)

## Read-only / copy-on-write protection on kernel-mediated copies (test74)

A POSIX `read(2)` into a read-only `mmap`'d buffer must fail with `EFAULT`; on
amd64 it silently succeeded.  Three independent defects let kernel-mediated
writes bypass a page's write protection — all three had to be fixed:

1. **`createpde()` ignored the PTE write bit.**  It returns the kernel
   direct-map alias (`phys_to_kacc(phys)`, always writable) for any *present*
   page, so a kernel-mediated write into a present read-only or COW page wrote
   straight through — no fault, no copy-on-write.  (i386 avoids this with
   `CR0.WP=1` plus its transplanted process-PDE copy window.)  Fix:
   `createpde()`'s 4th parameter is now a `writable` flag (formerly the unused
   `free_pde_idx`); `lin_lin_copy()` passes 0 for the source and 1 for the
   destination — see the Theme 2 snippet above.  For a write, `createpde()`
   returns the not-present sentinel `0` when the leaf entry lacks
   `AMD64_VM_WRITE`, so the access routes through `vm_suspend(..., writeflag=1)`
   and VM performs copy-on-write (writable region) or returns `EFAULT`
   (read-only region).

2. **`CR0.WP` was never set.**  Long mode is entered with paging already on by
   the EFI loader, so `pre_init.c` *skipped* `vm_enable_paging()` — but that
   routine also sets `CR0.WP` (and `CR4.PGE`).  Without WP, supervisor-mode
   writes ignore read-only PTEs entirely (the whole protection model assumes
   WP=1).  Fix: call `vm_enable_paging()` after `pg_load()` on amd64; it only
   ORs in WP/PGE and does **not** re-toggle paging.

3. **`vm_memset()` had the same not-present bug as `lin_lin_copy()`** — this is
   the `safememset` path, used e.g. by `/dev/zero` reads.  It relied on
   `phys_memset` faulting, but a not-present page makes it touch virtual address
   0, whose caught fault address `0` is indistinguishable from "no fault", so
   the memset silently succeeds writing nowhere.  Fix: pass `writable=1` to
   `createpde()` and, exactly like `lin_lin_copy()`, explicitly
   `vm_suspend(..., 1)` when it returns 0.

Why all three: (1) covers the cross-process page-walk copy (a server's
`safecopy` into a user buffer), (2) covers a write through the faulting
process's own mapping, (3) covers `safememset`.  This also makes copy-on-write
correct for kernel-mediated writes generally, not just the test.

## FPU / XMM context-switch state (test2)

A POSIX `test2` pipe test failed with "wrong data": a parent filled a buffer with
an SSE-vectorized loop (`buf[i]=i&0xff` → clang emits `movdqa`/`paddb`, holding
the increment constant `{0,1,…,15}` in `xmm0` across the whole loop), wrote it
through a pipe, and the child read back corruption — every other 16-byte group
zeroed, as if `xmm0` had been clobbered to `0` (a `pxor %xmm0,%xmm0` signature)
mid-fill.  The root cause was **XMM not surviving a context switch**, from two
independent amd64 defects:

1. **`fpu_init()` mis-probed the FPU → no `fxsave`.**  The x87 probe
   (`fninit`/`fnstcw`) ran with `CR0.EM=1` (FPU emulation), so the control-word
   check failed and the code took the "no FPU" branch, leaving
   `osfxsr_feature=0`.  With that flag clear, `save_local_fpu()` used `fnsave`
   (x87/MMX only, 108 bytes) instead of `fxsave` — **the XMM registers were
   never saved/restored at all**.  SSE still ran in userland (`CR4.OSFXSR` was
   set elsewhere), so XMM silently leaked across every context switch.  Fix
   (`arch_system.c fpu_init`): clear `CR0.EM`/`CR0.TS` and set `CR0.MP` before
   the probe so x87 initializes and the probe succeeds → `osfxsr_feature=1` →
   `fxsave`/`fxrstor` save the full 512-byte area including all 16 XMM.

2. **`do_fork()` gave the child the parent's FPU save buffer.**  Each process's
   `p_seg.fpu_state` points at its own slot in `static fpu_state[NR_PROCS][512]`
   (assigned by `arch_proc_reset` from `p_nr`).  `do_fork` copies the parent
   `proc` struct wholesale (`*rpc = *rpp`), which **overwrites the child's
   `fpu_state` pointer with the parent's**; the code that saves the child's own
   pointer beforehand and restores it afterward (then `memcpy`s the parent's FPU
   contents in) was guarded by `#if defined(__i386__)` and so was **compiled out
   on amd64**.  Result: parent and child shared one FPU buffer.  The lazy-FPU
   `#NM` path then corrupted it: the child's first SSE use trapped, saved *its*
   `xmm0=0` into the shared buffer, and when the parent resumed `fxrstor` loaded
   that zero — exactly the observed `xmm0={0,1,2,3}` → `00000000` transition.
   Fix: change both guards to `#if defined(__i386__) || defined(__x86_64__)` so
   the child keeps its own buffer (declaration of `old_fpu_save_area_p` too).

Diagnosing this took a buffer-aliasing detector in `save_local_fpu`/`restore_fpu`
that printed each watched process's `p_nr` and `fpu_state` address: the parent
(`nr=205`) and child (`nr=210`) showing the *same* `buf=0x…` pinned it
immediately.  General lesson: any per-process resource the i386 port carries
across `*rpc = *rpp` in `do_fork` (here the FPU buffer pointer) must have its
`#if defined(__i386__)` guard extended to amd64.

## Limitations / future work

- **> 64 GB RAM**: identity map covers up to `PG_IDENT_PD_MAX` (64) GB; each
  entry is a 512-entry PD covering 1 GB via 2 MB pages.  Increase
  `PG_IDENT_PD_MAX` (and add more `pg_pdpt_low` entries) to support more RAM.

## Companion POSIX-suite fixes (non-arch)

Other `minix-posix` failures fixed alongside the above, recorded here for the
trail (the code lives outside `kernel/arch`):

- **test53** — `minix/include/minix/u64.h`: `ex64lo()`/`ex64hi()` returned
  `unsigned long`, which is 64-bit on amd64, so they no longer truncated to the
  low/high 32 bits.  Fixed by masking through `(u32_t)` while keeping the
  `unsigned long` return type (preserves i386 semantics and `%lx` callers).
- **test66** — `minix/tests/test66.c` used `unsigned long`/`signed long` for
  values meant to be 32-bit; on LP64 that changes `(unsigned long)(-10) % 7`.
  Switched those locals to `int32_t`/`uint32_t`.
- **test64 panic / test74** — see "Read-only / COW protection" above and the
  low-memory reserve in `docs/vm-x86_64-port.md`.
- **test76** — silenced an upstream debug `printf` in `servers/vfs/worker.c`.
- **test77** (PTY) — the earlier subtest-5 `sigsuspend` heisenbug-hang cleared
  with the WP/COW and FPU fixes above (the suite now runs through to the end),
  exposing a real subtest-7 failure: `test_getdents` over `/dev/pts` checks that
  each Unix98 slave node belongs to the `tty` group, but they came up group
  `wheel` (gid 0).  Root cause was in `etc/MAKEDEV.tmpl`: the `ptmx)` recipe
  created `/dev/ptmx` with no group argument, so `mkdev` defaulted it to
  `$g_wheel`.  MINIX `etc/usr/rc` then starts the pty driver with
  `-args "gid=$(stat -f '%g' /dev/ptmx)"`, so the driver stamped every slave
  node (via ptyfs) with gid 0 instead of `tty` (4).  Fix: add `$g_tty` to the
  `ptmx` line — `mkdev ptmx c %ptmx_chr% 0 666 $g_tty`.  NetBSD's upstream
  template omits the group because NetBSD does not derive the tty gid from
  `/dev/ptmx`; that mechanism is MINIX-specific, so the group is required here.
  Machine-independent (would affect i386 too).  **Resolved:** after the
  live-image rebuild recreated `/dev` with the corrected `ptmx` group, test77
  passes end-to-end.

## Known limitation — test85 (vnd block-device EOF) deadlocks on a single-partition image

`test85` sets up a `vnd` whose backing file (`image`) is created in the test's
working directory (`/usr/tests/minix-posix`), then does block I/O on
`/dev/vnd0`.  On the dev-efi3 **emuimage**, everything lives on one MFS
partition (`distrib/amd64/liveimage/emuimage/fstab.in`: only `/dev/c0d0p1 /
mfs`), which makes the test deadlock on its first device read:

1. `read(/dev/vnd0)` — for an *unmounted* block device, VFS routes the block I/O
   to `ROOT_FS_E` (`servers/vfs/open.c` `v_bfs_e = ROOT_FS_E`).  The **root MFS**
   runs `lmfs_bio` → `bdev_gather`, a **synchronous** sendrec to vnd
   (`lib/libbdev/bdev.c` `bdev_vrdwt`), and blocks.
2. vnd services that request by `pread()`-ing its backing file `image` — which is
   on the **same root MFS**.
3. That `pread` queues behind the blocked root MFS → deadlock (single-threaded
   FS can't re-enter itself).

This is **not arch-specific** and not an LP64 bug: the vnd / libbdev /
libblockdriver / `lmfs_bio` data path all compute correctly on amd64 (iovec math
verified; `iovec_t` and `iovec_s_t` are both 16 bytes with matching field
offsets).  A stock MINIX install avoids the deadlock only because `/usr` is a
**separate** MFS process, so vnd's backing-file reads never touch the root MFS
that is doing the block caching.  i386 with the same single-partition emuimage
hangs identically.

Resolutions, if ever needed: give the emuimage a separate `/usr` partition (matches
stock MINIX, fixes the whole suite's `/usr != /` assumption), or host the vnd
backing file on a different FS instance (e.g. a ramdisk MFS).  Left as a known
limitation for now; the earlier `test85` *crash* (vnd `iovec_s_t`/`iov_grant`
sign-extension at config time) is fixed — see commit `25297885a`.

*(APIC was ported in a prior session; see `docs/apic-x86_64.md`.)*
