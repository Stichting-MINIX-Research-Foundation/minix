# MXV Phase 2 RAM/EPT validation notes

This directory records the Phase-2 acceptance checks. The code deliberately
keeps EPTP detached from the Phase-1 VM-entry probe, so the checks below must
be run on an Intel VT-x/EPT machine after the Phase-3 guest VMCS integration
exists. They are not claimed as passing in this change.

## VM-server pin lifecycle

1. Allocate a page-aligned region using the existing contiguous anonymous
   memory type with preallocation enabled (MAP_CONTIG | MAP_PREALLOC).
2. Call vm_mxv_pin(addr, len, &token); verify the call succeeds only when
   the entire region is resident, writable, physically contiguous, and owned by
   the caller.
3. While pinned, verify munmap, partial unmap, and fixed replacement fail
   with EBUSY.
4. Call vm_mxv_unpin(addr, token); verify a second unpin, a stale token, a
   token for a different address, and a token from another endpoint fail.
5. Verify that an owner exit revokes kernel state before VM frees the backing
   pages; repeat with VM server restart and forced owner termination.

## Kernel EPT memslot lifecycle

1. Create a VM handle and attach a pin token at a page-aligned GPA.
2. Verify read/write/execute permissions are reflected in the generated EPT
   leaf entries and that write/execute are not implicitly enabled.
3. Verify zero-sized, unaligned, overflowing, overlapping, stale-generation,
   duplicate-pin, and out-of-range requests are rejected.
4. Detach the slot and verify the pin can then be released. Destroy a VM only
   after all slots are detached.
5. Repeat with multiple slots and VMs until the bounded global RAM/table budget
   returns ENOSPC, then verify rollback leaves earlier slots intact.
6. Terminate the owner with attached slots and verify all kernel EPT/pin
   bookkeeping is reclaimed before the address space is reused.

## Current limits

The Phase-2 kernel implementation currently uses a fixed EPT table pool and
caps *aggregate* pinned RAM across all VMs at 16 GiB (not 16 GiB per VM).
The initial implementation also bounds the system to 8 VM objects, 8 memslots
per VM, and 64 outstanding pin tokens. Replacing the fixed pool with a kernel
page allocator is a follow-up before relaxing these limits.

## Current limitation

Phase 2 builds kernel-owned EPT tables and validates their lifecycle, but the
Phase-1 VMLAUNCH probe still uses the host CR3. It does not install the EPTP
into a guest VMCS or expose guest RAM to a running guest. Guest RAM read/write,
INVEPT, vCPU quiesce, and real-hardware acceptance remain Phase-3 integration
and hardware-test gates.
