# x86_64 Driver Porting

## Overview

The MINIX driver build system originally gated the essential x86 driver set behind
`${MACHINE_ARCH} == "i386"`, leaving x86_64 with only the architecture-agnostic
drivers (memory, ramdisk, vnd, log, random). This document describes the changes
made to enable the essential driver subset for x86_64.

## Scope

Drivers enabled for x86_64 (essential x86 subset):

| Category | Drivers |
|----------|---------|
| Bus | `pci`, `ti1225` |
| Storage | `ahci`, `at_wini`, `floppy`, `fbd`, `filter`, `virtio_blk` |
| Network | `3c90x`, `atl2`, `dec21140A`, `dp8390`, `dpeth`, `e1000`, `fxp`, `ip1000`, `lance`, `rtl8139`, `rtl8169`, `virtio_net`, `vt6105` |
| HID | `pckbd` |
| Power | `acpi` |
| Audio | `als4000`, `cmi8738`, `cs4281`, `trident` (MMIO); `es1370`, `es1371`, `sb16` (port-I/O) |
| Printer | `printer` (Centronics/LPT) |
| IOMMU | `amddev` (AMD K8 Device Exclusion Vector) |
| TTY | already built unconditionally; `bios_console` removed from x86_64 |

Not included: `vmm_guest/vbox`.

## Audio drivers (LP64 audit)

All seven audio drivers build and stage for x86_64.  Two addressing classes:

- **MMIO** (`als4000`, `cmi8738`, `cs4281`, `trident`): needed the BAR /
  `base[6]` / `io.h` port-parameter widening from `u32_t` to `vir_bytes`
  documented under "PCI BAR address widening" above.
- **Port-I/O** (`es1370`, `es1371` via PCI I/O BAR; `sb16` via legacy ISA
  ports + 8237 DMA): no MMIO, so the `u32_t base` (an I/O-port BAR, always
  <64 KB) is correct as-is.  DMA is hardware-limited (es137x 32-bit PCI;
  sb16 24-bit ISA + page register = 16 MB), matching the `drv_set_dma(u32_t
  dma, ...)` framework signature.  `libaudiodriver` (`audio_fw.c`) allocates
  the DMA buffer with `AC_LOWER16M`, so `DmaPhys` is always <16 MB and the
  `u32_t` parameter never truncates on amd64.  All three compile clean under
  `-Werror` with no format/cast fixes required.

None of the seven is runtime-tested: QEMU's `pc`/`q35` machines emulate none
of this hardware by default (es137x needs `-device ES1370`, sb16 needs
`-device sb16`, etc.), so they simply do not probe on the standard test setup.

## Modern (virtio-1.0) virtio support

On `-machine q35` QEMU presents virtio devices as **modern / non-transitional**
virtio-1.0 devices, not the legacy/transitional ones used on `-machine pc`:

| | Legacy / transitional (pc) | Modern (q35) |
|---|---|---|
| PCI device ID | `1af4:1000` (net), `1af4:1001` (blk) | `1af4:1041` (net), `1af4:1042` (blk) |
| Register access | flat I/O BAR0 block | MMIO regions via vendor PCI caps |
| Queue address | single 32-bit PFN | 64-bit desc/avail/used triplet |
| Features | 32-bit | 64-bit, must ack `VIRTIO_F_VERSION_1` |

`libvirtio` was legacy-only, so on q35 the driver was never even bound (its
`.conf` listed only `1af4:1000`) and, if bound, would have failed with "PCI not
IO space". `minix/lib/libvirtio/virtio.c` now supports **both** interfaces:

- **Capability discovery** (`init_modern`): walks the PCI capability list for
  vendor caps (`PCI_CAP_ID_VNDR` = 0x09), reads each `virtio_pci_cap`
  (`cfg_type`/`bar`/`offset`/`length`), and `vm_map_phys()`-maps the common,
  notify, ISR and device-config MMIO windows (`map_cap_region`, which handles a
  non-page-aligned offset). Presence of a common-cfg cap ⇒ modern; otherwise
  `init_device` falls back to the legacy I/O BAR. Enables `PCI_CR` mem-space +
  bus-master and clears INTx-disable (`pci_reserve` does not touch `PCI_CR`).
- **Register/queue/notify/ISR/device-config** access branches on `dev->modern`;
  the legacy paths are byte-for-byte unchanged. Modern queue setup programs the
  64-bit desc/avail/used addresses derived from the single contiguous vring
  allocation, sets `queue_enable`, and notifies at
  `notify_base + queue_notify_off * notify_off_multiplier`.
- **Status handshake**: reset → ACK → DRIVER → features → (modern) FEATURES_OK +
  verify → queues → DRIVER_OK.
- **Matching** (`is_matching_device`): legacy = PCI ID `0x1000-0x103f` with the
  subsystem ID equal to the expected virtio type; modern = PCI ID
  `0x1040 + type`. Both `.conf`s gained the modern ID (`virtio_net.conf` →
  `1af4:1041`; `etc/system.conf` virtio_blk → `1af4:1042`).

Driver-visible difference: the virtio-1.0 net header always includes
`num_buffers` (12 bytes) even without `MRG_RXBUF`, vs 10 bytes on legacy.
`virtio_net.c` queries the new `virtio_is_modern()` and sizes the header
(`net_hdr_size`) accordingly; header slots are always allocated at the 12-byte
`virtio_net_hdr_mrg_rxbuf` size so one allocation serves both.

INTx only (no MSI-X): queue/config MSI-X vectors are left at `NO_VECTOR`, and
`virtio_had_irq` reads the mapped ISR region. PCI INTx routing behind the q35
PCIe root port is the same path validated for e1000 (see `apic-x86_64.md`).

`virtio_net` also gained a `ndr_get_link` callback so `vio0` reports
`status: active`: it negotiates `VIRTIO_NET_F_STATUS` and reads the config
`status` field's `VIRTIO_NET_S_LINK_UP` bit (link assumed up if the device does
not expose status), reported as media `IFM_ETHER | IFM_AUTO`.

Confirmed working on q35 with the default modern virtio-net device: the driver
binds, reads its MAC, brings up `vio0`, and passes traffic. The legacy code
paths are structurally unchanged.

## Printer (Centronics/LPT)

The `printer` driver is port-I/O only (LPT data/status/control at `port_base`
+0/+1/+2) and grant-based for user transfers — no MMIO/DMA, LP64-clean as-is.
It was already UEFI-aware: `do_probe` reads `kinfo.boot_mode` and falls back to
the standard ISA LPT1 port `0x378` when booted under UEFI (no BIOS Data Area to
`sys_readbios`), otherwise reads `LPT1_IO_PORT_ADDR` (0x408) from the BDA.

The only blocker was that `<minix/drivers.h>` gated `#include <machine/bios.h>`
(for `LPT1_IO_PORT_ADDR`) and `<machine/ports.h>` on `#if defined(__i386__)`.
Both headers exist and are valid for x86_64 (same x86 port I/O; `ports.h`
already had its own `__i386__ || __x86_64__` guard), so the include guard in
`drivers.h` was widened to `defined(__i386__) || defined(__x86_64__)`.  The
full x86_64 `minix/drivers` tree rebuilds clean with no macro-redefinition
collisions from the newly-included `bios.h`/`ports.h`.

## IOMMU (AMD DEV) *(enabled, LP64-clean)*

`iommu/amddev` is the driver for the **AMD Device Exclusion Vector** (DEV), the
pre-AMD-Vi DMA-protection facility on K8/early-K10 northbridges (PCI
`1022:1103`, the HyperTransport "Miscellaneous Control" function). It is *not*
the modern AMD-Vi / Intel VT-d remapping IOMMU — DEV only gates device DMA
through a per-page bitmap. The `Makefile` guard was extended from `i386` to
`i386 || x86_64`.

LP64 audit: the IOMMU_MAP wire protocol is already 64-bit-safe — senders
(`fxp`, `rtl8139`) pass the buffer address and size through `m2_l1`/`m2_l2`
(`long`, 64-bit on amd64) and the PCI bus/dev/func through the `int` `m2_i*`
fields; `amddev` reads them back as `m1_i1..3`, which alias the same offsets, so
nothing truncates. The DEV base register is programmed via the `DEVF_BASE_HI` /
`DEVF_BASE_LO` 32-bit halves, so >4 GB bitmap placement is representable.

The only build fixes were four `-Wformat` errors where `size_t` (`unsigned
long` on amd64) was printed with `%x`; changed to `%zx` (correct on both
arches). Builds and links clean under `-Werror` as a 64-bit static ELF and
stages to `service/amddev`; added to the `minix-base` / `minix-debug` amd64 set
lists. Not runtime-testable on QEMU (no machine emulates the AMD DEV
northbridge), so it is build/LP64-verified only — like the audio drivers.

## Files Changed

### Makefile MACHINE_ARCH guards

Each `.if ${MACHINE_ARCH} == "i386"` block was extended to
`.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "x86_64"`:

| File | Notes |
|------|-------|
| `minix/drivers/bus/Makefile` | `pci`, `ti1225` blocks |
| `minix/drivers/storage/Makefile` | two blocks: MKIMAGEONLY-gated + unconditional |
| `minix/drivers/net/Makefile` | all net drivers block |
| `minix/drivers/hid/Makefile` | `pckbd` block |
| `minix/drivers/power/Makefile` | `acpi` block |
| `minix/drivers/storage/ramdisk/Makefile` | PROGRAMS block; `rs.single` kept i386-only (real-mode BIOS) |

### Inline `#ifdef` guards

`#if defined(__i386__)` extended to `#if defined(__i386__) || defined(__x86_64__)`:

| File | What was re-enabled |
|------|---------------------|
| `minix/drivers/tty/tty/arch/x86_64/rs232.c` | `port_t` struct fields, `addr_8250[]` table, B50/B1800/B38400–B115200 baud rates |
| `minix/drivers/storage/memory/memory.c` | `sys_enable_iop()` for `/dev/mem` |
| `minix/drivers/storage/ramdisk/proto` | storage/HID/isofs/acpi binaries in ramdisk image |

### Build system MK* variables (`share/mk/bsd.own.mk`)

`MKPCI`, `MKACPI`, `MKAPIC`, and `MKDEBUGREG` were defaulted `yes` only for
`i386`, so on x86_64 `libsys` skipped all 18 `pci_*.c` client stubs and any
driver calling `pci_init()` / `pci_get_bar()` etc. failed to link. The fix
extends the `_MKVARS.yes` block to `i386 || x86_64` for those four variables.
`MKWATCHDOG` and `MKINSTALLBOOT` remain i386-only.

Side effect: `MKACPI=yes` on x86_64 now causes the ramdisk Makefile's
`${MKACPI} != "no"` guard (already inside the `i386 || x86_64` block) to
include the `acpi` service in the ramdisk image.

### TTY x86_64 arch

`minix/drivers/tty/tty/arch/x86_64/Makefile.inc`: removed `bios_console.c`
(BIOS video services are not available under UEFI/64-bit boot).

### Type fixes in driver sources

| File | Fix |
|------|-----|
| `minix/drivers/bus/pci/pci.c` | `(u32_t)` cast on `kinfo.mem_high_phys` (`phys_bytes` = `unsigned long`, 64-bit on x86_64) in `complete_bars()` — silenced implicit truncation warning |
| `minix/drivers/net/3c90x/3c90x.c` | Pointer cast via `uintptr_t`; format string fixes |
| `minix/drivers/net/atl2/atl2.c` | Pointer cast via `uintptr_t` |
| `minix/drivers/net/dp8390/dp8390.c` | Pointer cast via `uintptr_t` |
| `minix/drivers/net/e1000/e1000.c` | Pointer cast via `uintptr_t` |
| `minix/drivers/net/fxp/fxp.c` | Format string fixes |
| `minix/drivers/net/lance/lance.c` | Pointer cast via `uintptr_t`; format string fixes |

## Format string mismatches in storage drivers *(fixed)*

On x86_64, `u64_t` is `unsigned long`, not `unsigned long long`.  `%llu` /
`%llx` with a `u64_t` argument causes a `-Wformat` error.  All five cases were
fixed by replacing with `PRIu64` / `PRIx64` macros (and `#include <inttypes.h>`
where missing):

| File | Line | Was | Fix |
|------|------|-----|-----|
| `minix/drivers/storage/ahci/ahci.c` | 392, 1426 | `%llu` | `%"PRIu64"` |
| `minix/drivers/storage/ahci/ahci.c` | 1154 | `%llx` | `%"PRIx64"` |
| `minix/drivers/storage/virtio_blk/virtio_blk.c` | 279 | `%016llx` | `%016"PRIx64"` |
| `minix/drivers/storage/virtio_blk/virtio_blk.c` | 365 | `%llu` | `%"PRIu64"` |
| `minix/drivers/storage/filter/driver.c` | 776 | `%llx` | `%"PRIx64"` (inside `#if DEBUG2`) |

### MMIO virtual address truncation in ip1000 and vt6105 *(fixed)*

Both drivers called `vm_map_phys()` to map device MMIO and stored the 64-bit
virtual address via an explicit `(u32_t)` cast, silently discarding the upper
32 bits.  The truncated address was then used for all register accesses.

**Fix applied**: widened the entire MMIO address chain from `u32_t` to
`vir_bytes` (`unsigned long`, 64-bit on x86_64) throughout both drivers:

- `NDR_driver.base[6]` (`ip1000.h`, `vt6105.h`): `u32_t` → `vir_bytes`
- `base0` locals and `*base` parameters in all functions (`ip1000.c`, `vt6105.c`)
- `read_eeprom`, `read_phy_reg`, `write_phy_reg` base parameters (`ip1000.c`)
- Assignment: `pdev->base[i] = (vir_bytes)reg` (removing the truncating cast)
- `my_inb/inw/inl/outb/outw/outl` `port` parameters (`ip1000/io.h`, `vt6105/io.h`)

The `(volatile u8_t *)(port)` casts inside the io.h functions continue to work
correctly once `port` carries the full 64-bit VA.

### Already-correct drivers

| Driver | Notes |
|--------|-------|
| `dec21140A` | Uses I/O ports, not MMIO; all bus addresses are `u32_t` matching the hardware |
| `at_wini`, `floppy`, `virtio_net`, `dpeth` | No format or cast issues found |

## DMA 64-bit physical-address audit (all drivers)

On amd64 `phys_bytes` is 64-bit and `alloc_contig` / `sys_vumap` can return a
buffer above 4 GB. A driver that programs a hardware DMA descriptor or base
register must therefore write the **upper 32 bits**, not hardcode them to zero.
Several MINIX drivers, written for 32-bit i386, did exactly that. Audit result:

| Driver | HW DMA width | Status |
|--------|--------------|--------|
| `ahci` | 64-bit (CAP.S64A) | **fixed** — DBAU/CTBAU/FBU/CLBU upper halves (see AHCI section); warns if HBA lacks S64A |
| `e1000` | 64-bit | **fixed** — `RDBAH`/`TDBAH` were hardcoded `0`, and per-descriptor `buffer_h` was never written; both now carry `(u32_t)((u64_t)phys >> 32)` |
| `rtl8169` | 64-bit | **fixed** — ring-base `RDSAR_HI`/`TNPDS_HI` were `0` and the descriptor `addr_high` field was declared but never set; both now programmed |
| `ip1000` | 64-bit | already correct — descriptor `frag_info`/`next` are `u64_t` and store the full physical address |
| `virtio_blk`, `virtio_net` | 64-bit (modern) | already correct — `libvirtio` programs 64-bit desc/avail/used addresses (see virtio section) |
| `lance`, `floppy` | ISA (≤16 MB) | safe — buffers allocated with `AC_LOWER16M`, never above 16 MB |
| `es1370`, `es1371`, `sb16`, `als4000`, `cmi8738`, `cs4281`, `trident` | audio | safe — `libaudiodriver` allocates the DMA buffer with `AC_LOWER16M` |
| `dp8390`/`dpeth` | ISA PIO | safe — programmed I/O, no bus-master DMA |
| `at_wini` | IDE bus-master | safe — PRD `prdte_base` is fixed `u32_t` (8-byte entry); bails to PIO for buffers > 4 GB |
| `fxp` (i82557), `3c90x` (Vortex/Boomerang), `atl2`, `dec21140A` (Tulip), `rtl8139`, `vt6105` (Rhine) | **32-bit only** | inherently ≤ 4 GB — descriptors/registers have no upper-32-bit field. Correct on a < 4 GB host (and always on i386); on a > 4 GB host they would need DMA buffers below 4 GB, for which there is currently no `AC_LOWER4G` allocation primitive. Left as a known limitation; not the boot/test path |

The three **fixed** drivers (`ahci`, `e1000`, `rtl8169`) are behavior-identical
below 4 GB — the high half is `0`, exactly as before — so there is no i386
regression and no change on the QEMU/OVMF test setup, which keeps these buffers
low. The fix only adds correctness when a buffer legitimately lands above 4 GB
on a 64-bit-capable controller. The `(u64_t)` cast before `>> 32` keeps the
shift defined on i386 (where `phys_bytes` is 32-bit, yielding `0`).

**Verified**: a live image rebuilt with the AHCI refactor and the `e1000` /
`rtl8169` fixes boots cleanly on q35/UEFI (root mounts via AHCI) with no
regression — as expected, since the QEMU guest keeps all DMA buffers below 4 GB
and the changes are identical to the prior code on that path.

## PCI BAR address widening *(fixed)*

`pb_base` in `bus/pci/pci.c`, the IPC reply field
`mess_pci_lsys_busc_get_bar.base`, and `pci_get_bar()` were all 32-bit,
truncating MMIO BARs above 4 GB on x86_64.

**Fix applied**:

- `mess_pci_lsys_busc_get_bar.base`: `int` → `uint64_t`; `size_t size` →
  `uint32_t size`; `padding[44]` → `padding[40]` (maintains 56-byte size on
  i386; fits within 88-byte x86_64 payload)
- `pb_base` in `pci.c` struct: `u32_t` → `u64_t`
- `record_bar()`: `bar_high` saves the upper 32-bit DWORD for 64-bit BARs;
  `pb_base` is set to `bar | ((u64_t)bar_high << 32)`.  On i386 the original
  "ignore BAR if high bits set" guard is kept via `#if !defined(__x86_64__)`.
- `complete_bars()` 32-bit gap loops: skip BARs with `pb_base > 0xFFFFFFFFULL`.
- **`PBF_INCOMPLETE` fix** (`pci.c:1108`): the incomplete check was
  `if (bar == 0)`, testing only the low 32 bits.  A 64-bit BAR whose firmware
  address has its low DWORD equal to zero (e.g. a BAR at exactly `0x100000000`)
  has `bar == 0` but `bar_high == 1`, so `pb_base = 0x100000000`.  The old
  check wrongly set `PBF_INCOMPLETE`, and `complete_bars()` then skipped it
  (because `pb_base > 0xFFFFFFFF`), leaving `PBF_INCOMPLETE` set forever;
  `_pci_get_bar()` returned `EINVAL` and the device failed to initialise.
  Fixed by testing the full 64-bit combined value:
  ```c
  if ((bar | ((u64_t)bar_high << 32)) == 0)
  ```
- `_pci_get_bar()` / `pci_get_bar()` / `syslib.h` declaration: `u32_t *base`
  → `u64_t *base`
- All callers widened: `ahci.c`, `atl2.c`, `3c90x.c`, `e1000.c`, `ip1000.c`,
  `vt6105.c`, `als4000.c`, `cmi8738.c`, `cs4281.c`, `trident.c`
- Audio driver MMIO chain also fixed: `io.h` `port` params, `base[6]` struct
  field, `*base` / scalar `base` function params, and `(vir_bytes)reg` cast
  throughout `als4000`, `cmi8738`, `cs4281`, `trident`

## `paddr_t` in wsdisplayvar.h *(fixed for i386 and amd64)*

`minix/drivers/tty/tty/wscons/wsdisplayvar.h` uses `paddr_t` in a function
pointer declaration (the `mmap` member of `struct wsdisplay_accessops`).
`paddr_t` is a kernel/privileged type whose availability differs by arch:

| Arch | Condition for `paddr_t` in `machine/types.h` |
|------|----------------------------------------------|
| i386 | `_NETBSD_SOURCE && !_KERNEL` → `__uint64_t` (always in userland) |
| amd64 | `_NETBSD_SOURCE && (_KERNEL \|\| _KMEMUSER \|\| _KERNTYPES \|\| _STANDALONE)` only |

**i386 problem**: the old fallback `typedef unsigned long paddr_t` conflicted
with the `__uint64_t` definition already provided by `machine/types.h` (pulled
in via `sys/device.h` → `sys/mutex.h` → `sys/types.h` → `machine/types.h`).

**amd64 problem**: in MINIX userspace drivers none of `_KERNEL`, `_KMEMUSER`,
`_KERNTYPES`, or `_STANDALONE` is defined, so `machine/types.h` does not
define `paddr_t` at all.  The old fallback was also gated on `!defined(_KERNEL)
&& !defined(_KERNTYPES)`, so it would have fired — but `unsigned long` is the
correct 64-bit type on amd64 anyway.  The conflict arose only on i386.

**Fix** (`wsdisplayvar.h` lines 38–47):

```c
/* Ensure paddr_t is defined. i386 machine/types.h provides it in userspace;
 * amd64 gates it on _KERNEL/_KERNTYPES, so provide a fallback for MINIX
 * userspace drivers. */
#if !defined(_I386_MACHTYPES_H_) && !defined(_X86_64_TYPES_H_)
#include <machine/types.h>
#endif
#if defined(__x86_64__) && \
    !defined(_KERNEL) && !defined(_KMEMUSER) && !defined(_KERNTYPES) && \
    !defined(_STANDALONE)
typedef unsigned long paddr_t;
#endif
```

- The explicit `#include <machine/types.h>` (guarded by each arch's include
  guard) ensures i386 userspace gets its `__uint64_t` definition without
  conflict.
- The `__x86_64__` + no-kernel-flags fallback provides `unsigned long`
  (64-bit) for amd64 MINIX userspace drivers where `machine/types.h` would
  otherwise leave `paddr_t` undefined.
- Kernel builds on both arches already have `paddr_t` from their own include
  chain; neither branch interferes.

## AHCI / SATA bring-up on q35/UEFI *(fixed)*

q35 (the default UEFI machine on QEMU/KVM) has **no legacy IDE**: its disk is
AHCI/SATA, so `at_wini` (legacy PIO, ports 0x1F0/0x170) reads 0x00 status and
fails IDENTIFY. Booting from disk under pure UEFI therefore requires the `ahci`
driver, which had several latent LP64 bugs that only surfaced on amd64. The
driver is started in place of `at_wini` by `ahci=yes` (ramdisk `rc`); the disk
is the built-in q35 SATA controller at PCI 00:1f.2, port 0.

Full chain of fixes (all in `minix/drivers/storage/ahci/ahci.c` unless noted).
Note PCI interrupt routing (ACPI `_PRT` + IOAPIC GSI) is a prerequisite and is
documented in `apic-x86_64.md`.

### `iovec_s_t` type pun — the root-mount blocker

`bdr_transfer` (libblockdriver) passes an `iovec_t`, whose `iov_addr`
(`vir_bytes`, 64-bit) holds **either** a local virtual address (when
`endpt == SELF`) **or** a grant ID. `at_wini` reads `iov_addr` directly. AHCI
instead cast the vector to `iovec_s_t *` and read `iov_grant`:

```c
struct { vir_bytes iov_addr;  vir_bytes iov_size; } iovec_t;     /* offset 0: 8 bytes */
struct { cp_grant_id_t iov_grant; vir_bytes iov_size; } iovec_s_t; /* offset 0: 4 bytes */
```

On **i386** `iov_addr` and `iov_grant` are both 4 bytes at offset 0, so the pun
was harmless. On **amd64** `iov_grant` is the *low 32 bits* of the 64-bit
`iov_addr`, and the SELF path then sign-extended it:

```diff
-static int setup_prdt(... iovec_s_t *iovec ...)
+static int setup_prdt(... iovec_t *iovec ...)
 ...
-	if (endpt == SELF)
-		vvec[i].vv_addr = (vir_bytes) iovec[i].iov_grant;
-	else
-		vvec[i].vv_grant = iovec[i].iov_grant;
+	if (endpt == SELF)
+		vvec[i].vv_addr = iovec[i].iov_addr;
+	else
+		vvec[i].vv_grant = (cp_grant_id_t) iovec[i].iov_addr;
```

A SELF buffer at a high user address (`0xefbae000`, bit 31 set) became
`0xffffffffefbae000`, so `sys_vumap` → VM `handle_memory` found no region and
returned `EFAULT`. This hit the **partition-table read at open** (drvlib
`partition()` issues a `SELF` transfer into a high `mmap`'d buffer), so the
device never opened and root never mounted. Grants kept working throughout
because grant IDs are small positive ints (no high bits, no sign extension).

The fix changes `sum_iovec`, `setup_prdt`, and `port_transfer` parameters from
`iovec_s_t *` to `iovec_t *` and drops the `(iovec_s_t *)` cast in
`ahci_transfer`. `iov_size` is at the same offset in both structs, so the
size-summing path was unaffected.

### DMA descriptor 64-bit address fields

The AHCI HBA structures carry 64-bit physical addresses as low/high DWORD
pairs, but the driver wrote only the low half and hardcoded the upper half to
0. On amd64 `phys_bytes` is 64-bit, so any buffer above 4 GB (or, with
`alloc_contig`, anywhere) would DMA to the wrong address.

| Field | Location | Fix |
|-------|----------|-----|
| PRDT `DBAU` (data base, 63:32) | `ct_set_prdt` | was `*p++ = 0` → `*p++ = (u32_t)((u64_t)prdt->vp_addr >> 32)` |
| Cmd-list header `CTBAU` (63:32) | `port_set_cmd` | added `cl[3] = (u32_t)((u64_t)ps->ct_phys[cmd] >> 32)` |
| Port `FBU` (FIS base, 63:32) | `port_alloc` | `port_write(ps, AHCI_PORT_FBU, (u32_t)((u64_t)ps->fis_phys >> 32))` |
| Port `CLBU` (cmd-list base, 63:32) | `port_alloc` | `port_write(ps, AHCI_PORT_CLBU, (u32_t)((u64_t)ps->cl_phys >> 32))` |

The `(u64_t)` cast before `>> 32` keeps the shift well-defined on i386 (where
`phys_bytes` is 32-bit, the cast yields 0). Below 4 GB the upper halves are 0,
identical to the old behaviour, so there is no i386 regression. Above 4 GB they
program correctly on S64A-capable controllers (QEMU's ICH9 AHCI supports it).

**Refactor (post-bring-up).** The four ad-hoc low/high splits above were
replaced with shared helpers so the pattern lives in one place:

```c
#define ADDR_LO32(a)	((u32_t) (phys_bytes) (a))
#define ADDR_HI32(a)	((u32_t) ((u64_t) (phys_bytes) (a) >> 32))

#define port_write64(ps, r, a)	/* writes base reg r and upper reg r+1 */
```

`ct_set_prdt` / `port_set_cmd` use `ADDR_LO32`/`ADDR_HI32`; `port_alloc` uses
`port_write64(ps, AHCI_PORT_FB, …)` / `port_write64(ps, AHCI_PORT_CLB, …)`
(CLBU/FBU are the `+1` words). Byte-for-byte identical codegen on both arches;
the low-half writes are now explicit `u32_t` truncations rather than implicit
ones.

**S64A safety gap (documented, warned).** The driver now always programs the
upper-32-bit registers but has **no bounce-buffer path**, and there is no
`AC_LOWER4G` allocation primitive — DMA buffers (and user data pages from
`sys_vumap`) can land above 4 GB on a >4 GB host. An HBA that does not report
`CAP.S64A` ignores the upper-32-bit registers, so it can only be driven safely
below 4 GB. `ahci_init` now records `CAP.S64A` (`hba_state.has_s64a`) and prints
a warning at startup when 64-bit `phys_bytes` is in use but the controller lacks
S64A. QEMU's ICH9 reports S64A, so the supported path is unaffected; the warning
flags genuinely unsupportable hardware instead of corrupting silently.

### Device detection without a connect-change interrupt

Detection keyed solely on the **PxIS.PCS** bit (Port Connect *change* Status).
On QEMU's ich9 the disk is present and stable from power-on, so no
connect-change is ever latched (`PxIS.PCS == 0`) even though `PxSSTS.DET == 3`
plainly shows an established device. The spin-up-timeout path then wrongly
concluded `STATE_NO_DEV`.

```diff
-		if (port_read(ps, AHCI_PORT_IS) & AHCI_PORT_IS_PCS) {
+		if ((port_read(ps, AHCI_PORT_IS) & AHCI_PORT_IS_PCS) ||
+			(port_read(ps, AHCI_PORT_SSTS) & AHCI_PORT_SSTS_DET_MASK)
+				== AHCI_PORT_SSTS_DET_PHY) {
 			/* device is present; poll it instead of giving up */
 			ps->state = STATE_WAIT_DEV;
 			...
```

This generalises the existing "bad controller, no interrupt" (VirtualBox)
fallback to any controller that presents `SSTS.DET == DET_PHY` without raising
the connect interrupt.

### PCI command register

`pci_reserve()` adds the device's I/O/memory/IRQ to the driver's privileges but
does **not** touch the PCI command register. `ahci_init` now explicitly enables
memory space + bus mastering and clears the Interrupt-Disable bit (bit 10),
rather than relying on firmware state:

```c
u16_t cr = pci_attr_r16(devind, PCI_CR);
cr |= PCI_CR_MEM_EN | PCI_CR_MAST_EN;	/* mem space + busmaster (DMA) */
cr &= ~PCI_CR_INT_DIS;			/* enable INTx */
pci_attr_w16(devind, PCI_CR, cr);
```

`PCI_CR_INT_DIS` (0x0400) is defined locally in `ahci.c` (it is not in
`machine/pci.h`). On OVMF this is a no-op (CR is already `0x0007`), but it is
correct not to depend on firmware leaving INTx/bus-master enabled.

### Disk device naming (`/dev/cCdD`) and the MAKEDEV major bug *(fixed)*

MINIX names disks `/dev/c<C>d<D>` where **C is the controller** (selects the
block major via `CTRLR(n)` in `minix/include/minix/dmap.h`: 0→3, 1→8, 2→10,
3→12) and **D is the drive on that controller** (selects the minor:
`D * DEV_PER_DRIVE`, `DEV_PER_DRIVE = 5`). A single driver instance owns one
major (one controller) and serves all its drives/partitions as minors.

`etc/MAKEDEV.tmpl` computed the major from the **wrong digit**: it extracted
only the `d` digit (`expr $i : '...\(.\)'`, the 4th character) and passed it to
both the minor *and* `disk_major`, never reading the controller digit. So
`/dev/c1d0` and `/dev/c0d0` both resolved to **major 3, minor 0** — the same
device — and `gpt show /dev/c1d0` returned byte-identical output (same partition
GUIDs) as `c0d0`. The fix reads the controller digit separately
(`ctrlr=\`expr $i : '.\(.\)'\``) and majors off it, in all three disk node
classes (whole-disk, `pN` primary partitions, `pNsM` subpartitions):

```sh
ctrlr=`expr $i : '.\(.\)'`     # c<N>: controller -> major
disk=`expr $i : '...\(.\)'`    # d<N>: drive       -> minor
minor=$(($disk * 5))
disk_major ${ctrlr}
```

**Naming consequence on q35:** the single ICH9 AHCI HBA presents its SATA ports
as drives of *one* controller, so a second disk is `/dev/c0d1` (major 3, minor
5), **not** `/dev/c1d0`. `c1d0` is a second controller (major 8); with only
`ahci_0` started it has no bound driver and correctly fails to open instead of
aliasing `c0d0`.

### `gpt(8)` sector size on MINIX *(fixed)*

`gpt`'s `gpt_open()` queries geometry with NetBSD's `DIOCGSECTORSIZE` /
`DIOCGMEDIASIZE` ioctls, which MINIX block drivers do not implement (they expose
geometry via `DIOCGETP` → `struct part_geom`, byte `size`; logical sector size
is a fixed 512). The unsupported ioctl returned `ENOTTY` ("Inappropriate ioctl
for device"). `sbin/gpt/gpt.c` now has a `__minix` branch that uses `DIOCGETP`
(`<minix/partition.h>` / `<sys/ioc_disk.h>`), mirroring libc `minix_sizeup()`.
`gpt` is wired into the MINIX build via `sbin/Makefile` and
`distrib/sets/lists/minix-base/mi`.

## `IRQ_REENABLE` interrupt storm under the IOAPIC (SMP) *(pckbd, at_wini, rs232 — fixed)*

Under SMP (IOAPIC), a driver that registers its IRQ with `IRQ_REENABLE` and whose
device holds the IRQ line asserted until it is read (8042 `OBF`, legacy-IDE status)
storms on VirtualBox: the kernel auto-unmasks the IOAPIC pin right after notifying
the driver, before the driver has drained the device, and VBox re-fires the still-
asserted edge pin — livelocking the whole system before the driver can run.  Under
the non-SMP 8259 PIC this does not happen, so it looks like an SMP-only hang.  Fix
is **drain-before-reenable**: policy `0` instead of `IRQ_REENABLE`, then
`sys_irqenable()` *after* reading the device.  Applied to `pckbd` (keyboard; PS/2
mouse/AUX also left disabled), `at_wini` (legacy compat IRQ 14/15; `w_intr_wait`
re-enables after the `REG_STATUS` read, native drives unchanged), and `rs232`
(tty serial; `rs_interrupt()` re-enables after `rs232_handler()` drains the UART —
VBox's emulated COM2 holds IRQ 3 asserted, so an amd64 boot with COM2 enabled hung
right after `IRQ 3 handler registered by tty`; `IRQ DIAG` probe showed `irq=3`
flooding with `actids=0x0`).  Still latent in `dpeth`/`vbox` (also `IRQ_REENABLE`,
not boot-critical).  **Full root-cause analysis and the before/after diffs are in
`docs/kernel-arch-x86_64.md` #18.**

**Caveat — this fix regresses QEMU if applied naively (see #19).**  Policy `0`
leaves the pin masked from the interrupt until the driver re-enables it; on a
*correct* edge-triggered IOAPIC (QEMU/real hardware, unlike VBox) an edge that
arrives in that masked window is **lost**, wedging the device permanently (dead
keyboard/serial).  Each driver must therefore **drain fully** (loop, not one unit)
*and* **re-check the device after unmask**: `pckbd_intr()` loops `scan_keyboard()`
then re-checks `KB_STATUS`; `rs_interrupt()` re-checks the IIR after
`sys_irqenable()`; `w_hw_int()` now acks + re-enables **all** drives (compat
included) so a leftover interrupt on a compat drive can't strand IRQ 14/15.
