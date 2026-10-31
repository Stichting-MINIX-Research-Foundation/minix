# x86_64 IPC ABI Redesign

## Background

The original x86_64 MINIX port copied i386 assembly stubs verbatim. Every
layer of the IPC path was broken: 32-bit stack-based argument passing, 32-bit
message sizes, no AMD64 SYSCALL fast path, and truncated 64-bit stack
pointers in the per-CPU TSS setup. This document describes the complete
replacement implemented on the `dev-efi3` branch.

## Message Layout

| Offset | Size | Field |
|--------|------|-------|
| 0 | 4 | `m_source` (endpoint) |
| 4 | 4 | `m_type` |
| 8 | 88 | payload |
| **Total** | **96** | |

The payload grew from 56 bytes (i386) to 88 bytes, holding 11 × 8-byte
pointers/longs. `_MSG_PAYLOAD_SIZE` in each arch's `ipcconst.h` drives the
size of all `mess_*` unions in `ipc.h`, and a compile-time assertion
(`_ASSERT_message`) enforces `sizeof(message) == 96`.

## Entry Paths

### INT 33 — universal fallback

The `IPCVEC_INTR` (32) and `IPCVEC_UM` (35) INT gates work on all CPUs
without any MSR setup. Used by libc stubs (`_ipc.S`) and the usermapped
softint variants.

### SYSCALL — fast path

Per-CPU entry points `ipc_entry_syscall_cpu0` … `ipc_entry_syscall_cpu7`
are registered in LSTAR (one per CPU, selected by the boot MSR-setup code).
SYSCALL does not push SS/RSP/RFLAGS/CS/RIP onto the kernel stack; the entry
code builds that frame manually from `%r10` (user RSP saved by caller) and
`%r11` (user RFLAGS saved by CPU).

> **Note:** The SYSCALL MSR setup (`IA32_EFER.SCE`, `IA32_LSTAR`,
> `IA32_STAR`, `IA32_FMASK`) is wired in `setup_sysenter_syscall`
> (`arch_system.c`); the fast path is live. Programming `IA32_FMASK` to clear
> `IF` on entry was the prerequisite that made the SYSCALL path safe — see
> "SYSCALL / SYSENTER MSR setup" in `docs/kernel-arch-x86_64.md`. The INT 33
> path remains as the universal fallback.

### Kernel calls — always INT

`KERVEC_INTR` (32) and `KERVEC_UM` (34) use INT gates only; there is no
SYSCALL fast path for kernel calls. Both `usermapped_do_kernel_call_softint`
and `usermapped_do_kernel_call_syscall` issue `int $KERVEC_UM`.

## Register Convention

```
Userspace (SysV AMD64 ABI)    →    Kernel (do_ipc args)
  rdi = endpoint / table            rdi = call_nr   (r1)
  rsi = msg_ptr / count             rsi = endpoint  (r2)
  rdx = status_ptr                  rdx = msg_ptr   (r3)
```

The libc stubs (`_ipc.S`, `usermapped_glo_ipc.S`) shuffle registers before
the trap: `rdx←rsi, rsi←rdi, rdi←$OP`. For SENDA the mapping is
`rdx←rdi` (table), `rdi←$SENDA` (rsi=count unchanged).

After the trap:
- `%rax` = return code from `do_ipc`
- `%rbx` = IPC status (`IPC_STATUS_REG = bx`), valid after RECEIVE/SENDREC

## Kernel Stack

Each CPU's kernel stack top is stored in `k_percpu_stacks[cpu]` (type
`u64_t`, stride 8×cpu in assembly). `tss_init()` writes this value to
`tss[cpu].sp0` so the CPU switches to the kernel stack automatically on
every INT/SYSCALL trap. `arch_finish_switch_to_user()` stores `proc_ptr` at
`[tss.sp0 - 8]` before returning to user so the SYSCALL entry can reload it
with a single `movq` relative to the switched stack.

`CURR_PROC_PTR = 40` (5 × 8 bytes: the CPU interrupt frame push of
RIP/CS/RFLAGS/RSP/SS).

## GP Register Save/Restore

`SAVE_GP_REGS` and `RESTORE_GP_REGS` (defined in `sconst.h`) save and
restore 14 general-purpose registers on every kernel entry — including
r12–r15 (callee-saved in the SysV ABI). `%rbp` is reserved as the
`proc_ptr` pointer inside the kernel and is handled separately.

`pushaq`/`popaq` macros (15 regs × 8 = 120 bytes) are used in the nested
exception path only.

### IPC arg reload

`SAVE_PROCESS_CTX` calls `RESTORE_KERNEL_SEGS` which writes `%si`
(`movw $USER_DS_SELECTOR, %si`), clobbering the user endpoint value. To
avoid this, `ipc_entry_common` always reloads IPC arguments from `p_reg`
(DIREG/SIREG/DXREG) rather than trusting live registers.

## Files Modified

| File | Description |
|------|-------------|
| `minix/include/arch/x86_64/include/stackframe.h` | `reg_t = uint64_t`; r8–r15 added to `stackframe_s` |
| `minix/kernel/arch/x86_64/procoffsets.cf` | R8REG–R15REG offset members added |
| `minix/kernel/arch/x86_64/sconst.h` | Full 64-bit rewrite: macros, CURR_PROC_PTR, SAVE/RESTORE_GP_REGS, SAVE_PROCESS_CTX, SAVE_TRAP_CTX |
| `minix/kernel/arch/x86_64/klib.S` | `copy_msg_from/to_user`: 96-byte routines with fault-redirect labels |
| `minix/kernel/arch/x86_64/mpx.S` | Complete rewrite: per-CPU SYSCALL entries, IPC/kernel-call dispatch, exception handling, user context restore |
| `minix/lib/libc/arch/x86_64/sys/_ipc.S` | AMD64 IPC libc stubs (INT 33) |
| `minix/kernel/arch/x86_64/usermapped_glo_ipc.S` | Usermapped softint + syscall stubs for all IPC ops |
| `minix/lib/libc/arch/x86_64/sys/_do_kernel_call_intr.S` | Fixed: `int $KERVEC_INTR` with `%rdi = msg_ptr` |
| `minix/include/arch/i386/include/ipcconst.h` | Added `_MSG_PAYLOAD_SIZE 56` |
| `minix/include/arch/x86_64/include/ipcconst.h` | `_MSG_PAYLOAD_SIZE 88`; fixed header guard name |
| `minix/include/minix/ipc.h` | All `mess_*` unions use `_MSG_PAYLOAD_SIZE`; compile-time size assertion |
| `minix/kernel/arch/x86_64/protect.c` | `k_percpu_stacks[]` → `u64_t`; pointer cast fixed to `reg_t` |
| `minix/kernel/arch/x86_64/include/arch_proto.h` | `extern u64_t k_percpu_stacks[CONFIG_MAX_CPUS]` |

## Message Struct Alignment Fix

`mess_lc_vfs_mount` originally placed `int flags` as its first field, followed
by four `size_t` fields. On x86_64 the compiler inserts 4 bytes of implicit
alignment padding between `int` (4-byte aligned) and `size_t` (8-byte
aligned), growing the struct to 96 bytes — exceeding `_MSG_PAYLOAD_SIZE = 88`
and triggering a negative-array-size error in `_ASSERT_message`.

Fix: move `int flags` after the last `vir_bytes label` field. No implicit
padding is needed there since `int` can follow an 8-byte field without gap.
The layout is now correct on both architectures:

| Arch | Field sizes | Struct total |
|------|------------|-------------|
| i386 | 8 × 4 B + 4 B + 20 B padding | 56 B ✓ |
| x86_64 | 8 × 8 B + 4 B + 20 B padding | 88 B ✓ |

All callers (`libc/sys/mount.c`, `servers/vfs/mount.c`) access fields by name,
so the reorder is ABI-transparent within the MINIX build.

## Bugs Fixed During Implementation

1. **ipc_entry_common clobbered endpoint** — RESTORE_KERNEL_SEGS writes
   `%si`; fixed by reloading IPC args from p_reg after context save.
2. **restore_user_context used i386 stack-arg convention** — `mov 8(%rsp)`
   replaced with `movq %rdi, %rbp` (AMD64 first arg).
3. **PCREG/PSWREG overridden by RESTORE_GP_REGS** — moved the
   `%rcx = PCREG / %r11 = PSWREG` stores to after RESTORE_GP_REGS.
4. **k_percpu_stacks was u32_t** — truncated 64-bit kernel stack addresses.
5. **exception_handler called with i386 push convention** — converted to
   AMD64 register passing (`%rdi = is_nested, %rsi = frame_ptr`).
6. **copr_not_available missing `pop %rbp`** — stack was left 8 bytes
   unbalanced after `context_stop`, misaligning subsequent calls.
7. **kernel_call_entry_common read AXREG** — message pointer is in `%rdi`
   on AMD64 (DIREG), not `%rax` (AXREG).

## SYSCALL MSR Setup — Done

`setup_sysenter_syscall()` is implemented in
`minix/kernel/arch/x86_64/protect.c`.  It programs:

- `AMD_MSR_EFER` — sets `AMD_EFER_SCE` to enable `SYSCALL`/`SYSRET`.
- `AMD_MSR_STAR` — kernel/user CS selectors in bits [63:32].
- `AMD_MSR_LSTAR` — per-CPU 64-bit RIP of `ipc_entry_syscall_cpuN`.

It is called from:

- `smp_single_cpu_fallback()` macro (`arch_smp.h`) — BSP single-CPU path.
- `arch_smp.c` lines 223 and 305 — BSP and AP SMP paths.

The SYSCALL fast path is active on both single-core and multi-core boots.
