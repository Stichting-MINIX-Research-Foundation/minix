# APIC x86_64 Port

## Background

The x86_64 MINIX kernel inherited i386 APIC assembly verbatim.  All four APIC
assembly files (`apic_asm.S` for both arches) were byte-for-byte identical,
which meant the x86_64 copy used 32-bit calling convention, 32-bit register
names, `pusha`/`popa`, and `iret` — none of which are valid in AMD64 long mode.
Additionally, `hw_intr.h` had a hardcoded `kernel/arch/i386/apic.h` include
path, and `ioapic_read`/`ioapic_write` accepted `u32_t` base addresses that
silently truncated 64-bit `vir_bytes` MMIO pointers.

This document describes the fixes applied on the `dev-efi3` branch.

## Files Changed

| File | Change |
|------|--------|
| `minix/kernel/arch/x86_64/apic_asm.S` | Full rewrite for AMD64 ABI |
| `minix/kernel/arch/x86_64/sconst.h` | Added `pushaq`/`popaq`/`KERNEL_IRQ_RFLAGS_OFF` |
| `minix/kernel/arch/x86_64/mpx.S` | Removed now-duplicate macro definitions |
| `minix/kernel/arch/x86_64/include/hw_intr.h` | Fixed hardcoded i386 include path |
| `minix/kernel/arch/x86_64/apic.c` | Fixed MMIO base address types |
| `minix/kernel/arch/x86_64/acpi.c` | Full 64-bit XSDT table addresses |
| `minix/kernel/arch/x86_64/mb2_acpi2.c` | Deleted (unused experimental XSDT parser) |
| `minix/kernel/proto.h` | Added forward declaration for `struct sigframe_sigcontext` |

---

## apic_asm.S — AMD64 Rewrite

### CS displacement in TEST_INT_IN_KERNEL

In i386, a same-privilege interrupt pushes only three words (EIP, CS, EFLAGS),
so CS is at `4(%esp)`.  In x86_64 the CPU **always** pushes a full five-word
frame (RIP, CS, RFLAGS, RSP, SS) — even for ring-0 → ring-0 — so CS is always
at `8(%rsp)`.

```diff
-TEST_INT_IN_KERNEL(4, 0f)
+TEST_INT_IN_KERNEL(8, 0f)
```

### Calling convention: APIC_IRQ_HANDLER

i386 passes arguments on the stack; AMD64 passes the first integer argument in
`%rdi`/`%edi`.

```diff
-#define APIC_IRQ_HANDLER(irq)    \
-    push    $irq                ;\
-    call    irq_handle          ;\
-    add     $4, %esp            ;
+#define APIC_IRQ_HANDLER(irq)    \
+    movl    $irq, %edi          ;\
+    call    irq_handle          ;
```

### context_stop call

i386 passed `proc_ptr` (held in `%ebp`) by pushing it onto the stack.  AMD64
passes it as the first argument in `%rdi`, matching the SysV AMD64 ABI.  The
pattern mirrors what `mpx.S` already does for the PIC handlers:

```diff
-    push    %ebp
-    call    context_stop
-    add     $4, %esp
-    movl    $0, %ebp          /* for stack trace */
+    movq    %rbp, %rdi        /* context_stop(proc_ptr) */
+    push    %rbp
+    movq    $0, %rbp          /* for stack trace */
+    call    context_stop
+    pop     %rbp
```

### LAPIC_INTR_HANDLER

The i386 version loaded the handler address into a 32-bit register and called
it indirectly; the EOI address was loaded with a 32-bit `mov`:

```diff
-#define LAPIC_INTR_HANDLER(func)                   \
-    movl    $func, %eax                            ;\
-    call    *%eax                                  ;\
-    mov     lapic_eoi_addr, %eax                   ;\
-    movl    $0, (%eax)                             ;
+#define LAPIC_INTR_HANDLER(func)                   \
+    call    func                                   ;\
+    movq    lapic_eoi_addr(%rip), %rax             ;\
+    movl    $0, (%rax)                             ;
```

`lapic_eoi_addr` is `vir_bytes` (64-bit); the old 32-bit load would truncate
on systems where the LAPIC is mapped above 4 GB.  The store remains 32-bit
(`movl`) because the LAPIC EOI register is a 32-bit MMIO register.

### In-kernel interrupt path

AMD64 has no `pusha`/`popa`.  The macros `pushaq`/`popaq` (15 × `push`/`pop`
covering all GP regs except `%rsp`) are defined in `sconst.h` and shared with
`mpx.S`.

```diff
-    pusha
-    call    context_stop_idle
-    APIC_IRQ_HANDLER(irq)
-    CLEAR_IF(10*4(%esp))
-    popa
-    iret
+    pushaq
+    call    context_stop_idle
+    APIC_IRQ_HANDLER(irq)
+    CLEAR_IF(KERNEL_IRQ_RFLAGS_OFF(%rsp))
+    popaq
+    iretq
```

`KERNEL_IRQ_RFLAGS_OFF` is 136: after `pushaq` (15 × 8 = 120 bytes), the
CPU-pushed frame sits at `120(%rsp)`; RFLAGS is 16 bytes into that frame
(after RIP and CS), giving `120 + 16 = 136`.

### APIC_DEBUG dummy handlers

The i386 version pushed arguments for `panic()` on the stack.  AMD64 ABI:

```diff
-#define lapic_intr_dummy_handler(vect)     \
-    pushl   $vect                         ;\
-    push    $lapic_intr_dummy_handler_msg ;\
-    call    panic                         ;\
+#define lapic_intr_dummy_handler(vect)                      \
+    movl    $vect, %esi                                    ;\
+    leaq    lapic_intr_dummy_handler_msg(%rip), %rdi       ;\
+    call    panic                                          ;\
```

Each handler is ≤ 19 bytes; `.balign LAPIC_INTR_DUMMY_HANDLER_SIZE` (32) pads
to the stride used by `lapic_set_dummy_handlers()` when programming the IDT.

---

## sconst.h / mpx.S — Shared Macros

`pushaq`, `popaq`, and `KERNEL_IRQ_RFLAGS_OFF` were previously defined only
inside `mpx.S`.  They are now in `sconst.h` (included by both `mpx.S` and
`apic_asm.S`) and removed from `mpx.S` to avoid redefinition.

---

## hw_intr.h — Include Path Fix

```diff
-#include "kernel/arch/i386/apic.h"
+#include "kernel/arch/x86_64/apic.h"
```

---

## apic.c — MMIO Base Address Types

`ioapic_read` and `ioapic_write` accepted `u32_t ioa_base`.  On x86_64
`vir_bytes` is 64-bit, so passing an I/O APIC virtual address truncated
silently to 32 bits.

```diff
-static u32_t ioapic_read(u32_t ioa_base, u32_t reg)
+static u32_t ioapic_read(vir_bytes ioa_base, u32_t reg)

-static void ioapic_write(u32_t ioa_base, u8_t reg, u32_t val)
+static void ioapic_write(vir_bytes ioa_base, u8_t reg, u32_t val)
```

`ioapic_redirt_entry_write` was updated from `void *` to `vir_bytes`
consistently, and the `(u32_t)` casts at all call sites were removed.

A `%llu` format string for `u64_t` was fixed: on x86_64 `u64_t` is
`unsigned long`, not `unsigned long long`.  A cast to `(unsigned long long)`
makes the format portable across both i386 and x86_64.

---

## proto.h — sigframe_sigcontext Forward Declaration

`struct sigframe_sigcontext` is defined in `sys/arch/i386/include/frame.h` but
not in the amd64 equivalent.  Using an undeclared struct tag in a function
prototype creates a declaration scoped to the parameter list, which Clang
rejects with `-Werror,-Wvisibility`.  A file-scope forward declaration was
added immediately before the `fpu_sigcontext` prototype.

---

## acpi.c — Full 64-bit XSDT Support

The APIC/SMP code discovers LAPICs and I/O APICs by walking the ACPI MADT,
which it locates through `acpi.c`.  On UEFI x86_64, ACPI is revision 2: the
RSDP points at an **XSDT** whose entries are 64-bit physical addresses, unlike
the legacy **RSDT** with 32-bit entries.

`acpi.c` already read the XSDT for `revision == 2`, but immediately narrowed
every entry back to 32 bits before storing it:

```diff
-	sdt_count = (s - sizeof(struct acpi_sdt_header)) / sizeof(u64_t);
-	for (i = 0; i < sdt_count; i++) {
-		rsdt.data[i] = (u32_t)xsdt.data[i];	/* truncates >4 GB tables */
-	}
+	sdt_count = (s - sizeof(struct acpi_sdt_header)) / sizeof(u64_t);
```

The whole lookup path was 32-bit too (`rsdt.data[]` is `u32_t[]`,
`acpi_phys2vir` took/returned `u32_t`), so any table the firmware placed above
4 GB was silently corrupted.  Since `phys_bytes` is `unsigned long` (64-bit) on
amd64, the truncation was the only barrier.

The fix makes the resolved table address the single source of truth and keeps
it full-width:

- Added `phys_bytes base` to the per-table `sdt_trans[]` record.
- A unified post-read loop selects each entry as a full `phys_bytes`
  (`rsdt.data[i]` for RSDT, `xsdt.data[i]` for XSDT — no down-cast) and stores
  it in `sdt_trans[i].base`.
- `acpi_get_table_base()` returns `sdt_trans[i].base` instead of
  `(phys_bytes)rsdt.data[i]`.
- `acpi_phys2vir(u32_t)` widened to `acpi_phys2vir(phys_bytes)`; the
  header-read error format changed `0x%x` → `0x%lx`.

`sdt_count` stays bounded by `MAX_RSDT` (35): `acpi_read_sdt_at()` rejects an
XSDT whose length overflows the `xsdt.data[MAX_RSDT]` read buffer.

The unused, debug-laden experimental parser `mb2_acpi2.c` (a standalone
`acpi2_init()` that was never compiled or called) was deleted.

### XSDT struct alignment padding (#GP at first boot)

When the EFI64 path first reached `acpi_init()`, the table loop took a #GP
(non-canonical address) in `memcpy` reading `xsdt.data[i]`.  `struct
acpi_sdt_header` is exactly 36 bytes (4-aligned), but `struct acpi_xsdt`'s
`u64_t data[]` forces 8-byte alignment, so the compiler inserted **4 bytes of
padding** after the header — `data[]` started at struct offset 40.  The
on-disk XSDT packs its 64-bit table pointers immediately after the header
(offset 36).  `memcpy`'ing the raw table into the struct therefore misaligned
every entry by 4 bytes, yielding mangled non-canonical addresses → #GP when
dereferenced.  Fix: mark the struct `__packed` so `data[]` sits at offset 36.
Unaligned 64-bit reads are fine on x86.  (The RSDT's `u32_t data[]` is
naturally 4-aligned and needs no packing.)

**Limitation:** before VM is running, `acpi_phys_copy` reads a physical address
*as* a virtual one via the boot identity/direct map.  The address path is now
correct for >4 GB tables, but actually reaching such a table also requires that
high range to be mapped at access time.  On QEMU/OVMF, firmware keeps ACPI
tables in low reclaim memory (<4 GB), so this works today; truly-high tables
would need a temporary mapping in `acpi_phys_copy`.

---

## PCI Interrupt Routing — ACPI `_PRT` → IOAPIC GSI

This is required for **any** PCI device that uses INTx interrupts (AHCI, NICs,
etc.) on the UEFI/APIC path. It surfaced during AHCI bring-up on q35: the disk
controller's interrupt never reached the driver. (Driver-side AHCI fixes are in
`drivers-x86_64.md`; the kernel IOAPIC code below was already correct.)

The chain: the userspace **ACPI service** (`minix/drivers/power/acpi/pci.c`)
walks each PCI root/bridge `_PRT` to build a dev/pin → GSI table; the **pci**
driver queries it (`acpi_get_irq`) and writes the resolved GSI into the device's
`PCI_ILR`; the driver reads `PCI_ILR` and calls `sys_irqsetpolicy(gsi)`; the
**kernel** programs IOAPIC redirection entry `gsi`.

### `_PRT` fixed-buffer overflow (`acpi/pci.c`, `get_pci_irq_routing`)

`AcpiGetIrqRoutingTable` was given a fixed 4 KB stack buffer:

```diff
-	ACPI_BUFFER abuff;
-	char buff[4096];
-	abuff.Length = sizeof(buff);
-	abuff.Pointer = buff;
-	status = AcpiGetIrqRoutingTable(bridge->handle, &abuff);
-	if (ACPI_FAILURE(status))
-		return AE_OK;	/* silently! */
+	ACPI_BUFFER abuff;
+	abuff.Length = ACPI_ALLOCATE_BUFFER;	/* let ACPICA size it */
+	abuff.Pointer = NULL;
+	status = AcpiGetIrqRoutingTable(bridge->handle, &abuff);
+	if (ACPI_FAILURE(status))
+		return AE_OK;
	...
+	ACPI_FREE(abuff.Pointer);
```

q35's bus-0 `_PRT` (enumerating ~30 slots × 4 pins) **exceeds 4 KB**, so the
call returned `AE_BUFFER_OVERFLOW` and the helper returned `AE_OK` without
populating the routing table. Every `acpi_get_irq()` then returned -1, so the
pci driver left each device's `PCI_ILR` at the firmware default. For the AHCI
controller that default was legacy **IRQ 10**, but on q35 its INTx is wired to
IOAPIC **GSI 16** — so the kernel programmed the wrong pin and no interrupt was
ever delivered (the driver spun on command timeouts).

The fix uses `ACPI_ALLOCATE_BUFFER` so ACPICA allocates exactly the needed size,
and frees the buffer (and the `AcpiGetObjectInfo` result, previously leaked).
This is a **general** amd64 fix, not AHCI-specific — it had silently broken PCI
interrupt routing for all bus-0 devices.

### Kernel side was already correct

`ioapic_set_irq()` treats the IRQ number as a GSI directly — it finds the IOAPIC
whose `gsi_base ≤ irq < gsi_base + pins` and programs `pin = irq - gsi_base`. A
single 24-pin IOAPIC (gsi_base 0) therefore routes GSI 16–23 without change.
`set_irq_redir_low()` already sets the correct polarity/trigger by range: ISA
IRQs (<16) edge/active-high, PCI IRQs (≥16) **level/active-low**. So once ACPI
hands the pci driver the right GSI, delivery works end-to-end.

### Devices behind a PCI-to-PCI bridge — interrupt-pin swizzle *(fixed)*

The ACPI service's `do_map_bridge`/`find_bridge` still do not resolve
**secondary** PCI-to-PCI bridge buses, so `libacpi: acpi failed to map pci (0)
to pci (N) bridge` is printed for each bridge and a direct
`acpi_get_irq(secondary_bus, ...)` for a device behind a bridge returns -1.
That is now handled by the spec-standard fallback in the **pci** driver rather
than by fixing the ACPI service: when direct routing fails, `record_irq()`
calls `derive_irq()`, which swizzles the device's INTx pin up the bridge
hierarchy until it reaches a bus ACPI can route (normally the root bus, which
has a `_PRT`).

`derive_irq()` had a long-standing bug that made it a no-op: it derived the
swizzle *slot* from `pd_func` (the function number, 0–7) instead of `pd_dev`
(the device/slot number), so `(pd_func >> 3)` was always 0 and the swizzle
`(pin + slot) % 4` never rotated the pin. It also only swizzled **one** level,
so a device behind a *nested* bridge (e.g. q35 bus 2 behind bus 1 behind bus 0)
got no IRQ at all (the one-level query hit bus 1, which has no `_PRT` node).

Fix (`minix/drivers/bus/pci/pci.c`, `derive_irq`):

```c
/* swizzle with the device/slot number, and walk up every bridge level */
for (levels = 0; levels < NR_PCIBUS; levels++) {
    busind = get_busind(dev->pd_busnr);
    if (pcibus[busind].pb_type != PBT_PCIBRIDGE &&
        pcibus[busind].pb_type != PBT_CARDBUS)
        return -1;                       /* host bus: nothing to swizzle */
    parent_bridge = &pcidev[pcibus[busind].pb_devind];
    pin = (pin + dev->pd_dev) % 4;       /* PCI-to-PCI Bridge spec swizzle */
    irq = acpi_get_irq(parent_bridge->pd_busnr, parent_bridge->pd_dev, pin);
    if (irq >= 0)
        return irq;
    dev = parent_bridge;                 /* not routable here; go up one level */
}
```

Verified on q35/UEFI: an `e1000` placed behind a (nested) bridge now routes
(`IRQ 22 handler registered by e1000`) and comes up `status: active`; before the
fix it got no GSI. Everything on the root bus (bus 0, including the q35 AHCI at
00:1f.2) was already routing correctly via the root `_PRT`. The
`libacpi: acpi failed to map ... bridge` and `do_get_irq: out of range` printfs
are now cosmetic — the swizzle path carries the routing.

This is a machine-independent MINIX bug (i386 had it too); it only became
load-bearing on the q35/UEFI/APIC path, where disks and NICs commonly sit behind
PCIe root-port bridges.

---

## Architecture Notes

- The LAPIC and I/O APIC MMIO regions (0xFEE00000 and 0xFEC00000) are
  physically below 4 GB and identity-mapped by the x86_64 kernel, so the
  addresses currently fit in 32 bits.  Nonetheless, using `vir_bytes` is
  correct and avoids latent bugs if the mapping ever moves.

- **`pg_identity()` must cover the MMIO hole.**  At first boot, APIC init took a
  page fault at `0xFEE00080`: `prot_init()` replaces the head.S boot map with
  `pg_identity()`, which only mapped RAM up to `mem_high_phys` (~2 GB on the
  test VM) — leaving the LAPIC at `0xFEE00000` (~4 GB) unmapped.  Fix: in
  `pg_utils.c`, `pg_identity()` always maps at least the low **4 GB**
  (`if (num_pds < 4) num_pds = 4;`), so the LAPIC/IOAPIC/video MMIO is
  reachable.  Pages above `mem_high_phys` are already mapped uncacheable
  (`PG_PWT | PG_PCD`).

- `iret` is illegal in 64-bit mode; `iretq` pops a 64-bit RIP, CS, RFLAGS,
  RSP, SS frame.

- The APIC C code (`apic.c`) is otherwise architecture-neutral: LAPIC/IOAPIC
  register access is via 32-bit MMIO reads/writes regardless of CPU bitness.
