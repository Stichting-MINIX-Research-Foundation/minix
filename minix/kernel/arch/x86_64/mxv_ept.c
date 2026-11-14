/*
 * MXV Phase 2: VM-owned pin tokens and kernel-owned EPT memslots.
 *
 * Only VM server code may mint/release pins.  VM first proves that the
 * requested range is one complete, preallocated contiguous-physical region;
 * this layer independently verifies the owner's present, writable user PTEs.
 * Neither HPA nor an EPT page-table address is accepted from userland.
 *
 * The EPTP is intentionally not installed into the Phase-1 VMLAUNCH probe:
 * that probe reuses the host address space.  Phase 3 must stop the vCPU and
 * add EPTP to a guest VMCS before the tables can be used for guest execution.
 */
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <minix/mxvctl.h>

#include "kernel/kernel.h"
#include "kernel/proc.h"
#include "kernel/spinlock.h"
#include "arch_proto.h"
#include "archconst.h"

#define MXV_PAGE_SIZE             4096ULL
#define MXV_MAX_VMS               8
#define MXV_MAX_PINS              64
/*
 * A 16-GiB aggregate RAM budget needs at most 8192 4-KiB leaf tables plus
 * upper levels and per-slot fragmentation.  Keeping the pool bounded avoids
 * depending on a kernel runtime page allocator that this MINIX port lacks.
 */
#define MXV_EPT_TABLES            8256
#define MXV_EPT_ADDR_MASK         0x000ffffffffff000ULL
#define MXV_EPT_PERM_MASK         0x7ULL
#define MXV_EPT_WB                (6ULL << 3)
#define MXV_PHYS_DIRECTMAP_BASE   0xFFFFFF8000000000ULL
#define MXV_MAX_GPA               (1ULL << 48)
#define MXV_MAX_TOTAL_RAM_BYTES   (16ULL * 1024 * 1024 * 1024)

struct mxv_pin {
	int in_use;
	int attached;
	endpoint_t owner;
	u32_t generation;
	phys_bytes hpa;
	u64_t size;
};

struct mxv_memslot {
	int in_use;
	u32_t generation;
	u32_t flags;
	u64_t gpa;
	u64_t size;
	u64_t pin_token;
};

struct mxv_vm {
	int in_use;
	endpoint_t owner;
	u32_t generation;
	u32_t ept_pages_used;
	u64_t eptp;
	struct mxv_memslot slots[MXV_MAX_MEMSLOTS];
};

/* Kernel-owned, page-aligned, bounded EPT table pool (16-GiB aggregate RAM). */
static u64_t mxv_ept_pages[MXV_EPT_TABLES][512] __aligned(4096);
static u8_t mxv_ept_page_owner[MXV_EPT_TABLES];
static struct mxv_pin mxv_pins[MXV_MAX_PINS];
static struct mxv_vm mxv_vms[MXV_MAX_VMS];
static u32_t mxv_next_generation = 1;
static u64_t mxv_pinned_bytes;
SPINLOCK_DEFINE(mxv_ept_lock);

static u32_t
mxv_generation(void)
{
	u32_t generation = mxv_next_generation++;

	if (generation == 0)
		generation = mxv_next_generation++;
	if (mxv_next_generation == 0)
		mxv_next_generation = 1;
	return generation;
}

static int
mxv_endpoint_live(endpoint_t endpoint)
{
	int proc_nr;

	if (!isokendpt(endpoint, &proc_nr))
		return 0;
	return !isemptyp(proc_addr(proc_nr));
}

static u64_t
mxv_handle(unsigned int slot, u32_t generation)
{
	return ((u64_t)generation << 32) | (u64_t)(slot + 1);
}

static struct mxv_pin *
mxv_pin_lookup(endpoint_t owner, u64_t token)
{
	u32_t slot = (u32_t)token;
	u32_t generation = (u32_t)(token >> 32);
	struct mxv_pin *pin;

	if (slot == 0 || slot > MXV_MAX_PINS)
		return NULL;
	pin = &mxv_pins[slot - 1];
	if (!pin->in_use || pin->owner != owner ||
	    pin->generation != generation)
		return NULL;
	return pin;
}

static struct mxv_vm *
mxv_vm_lookup(endpoint_t owner, u64_t handle, unsigned int *index)
{
	u32_t slot = (u32_t)handle;
	u32_t generation = (u32_t)(handle >> 32);
	struct mxv_vm *vm;

	if (slot == 0 || slot > MXV_MAX_VMS)
		return NULL;
	vm = &mxv_vms[slot - 1];
	if (!vm->in_use || vm->owner != owner ||
	    vm->generation != generation)
		return NULL;
	if (index != NULL)
		*index = slot - 1;
	return vm;
}

/* EPT table HPAs are kernel-generated; the direct map is never user supplied. */
static u64_t *
mxv_ept_page(u64_t physical)
{
	if ((physical & (MXV_PAGE_SIZE - 1)) != 0 ||
	    (physical & ~MXV_EPT_ADDR_MASK) != 0)
		return NULL;
	return (u64_t *)(vir_bytes)(MXV_PHYS_DIRECTMAP_BASE + physical);
}

static void
mxv_ept_free_pages(unsigned int vm_index)
{
	unsigned int i;

	for (i = 0; i < MXV_EPT_TABLES; i++) {
		if (mxv_ept_page_owner[i] == vm_index + 1) {
			memset(mxv_ept_pages[i], 0, sizeof(mxv_ept_pages[i]));
			mxv_ept_page_owner[i] = 0;
		}
	}
	mxv_vms[vm_index].ept_pages_used = 0;
	mxv_vms[vm_index].eptp = 0;
}

static int
mxv_ept_alloc_page(unsigned int vm_index, u64_t *physical)
{
	unsigned int i;
	u64_t pa;

	if (physical == NULL)
		return EINVAL;
	for (i = 0; i < MXV_EPT_TABLES; i++) {
		if (mxv_ept_page_owner[i] != 0)
			continue;
		pa = vir2phys(mxv_ept_pages[i]);
		if ((pa & (MXV_PAGE_SIZE - 1)) != 0 ||
		    (pa & ~MXV_EPT_ADDR_MASK) != 0)
			return EFAULT;
		mxv_ept_page_owner[i] = vm_index + 1;
		memset(mxv_ept_pages[i], 0, sizeof(mxv_ept_pages[i]));
		mxv_vms[vm_index].ept_pages_used++;
		*physical = pa;
		return OK;
	}
	return ENOMEM;
}

static int
mxv_ept_get_pd(unsigned int vm_index, u64_t gpa, u64_t **pd_out)
{
	u64_t *pml4, *pdpt, *pd;
	u64_t entry, physical;
	unsigned int pml4i, pdpti;
	int r;

	if (pd_out == NULL || mxv_vms[vm_index].eptp == 0)
		return EINVAL;
	pml4i = (unsigned int)((gpa >> 39) & 0x1ff);
	pdpti = (unsigned int)((gpa >> 30) & 0x1ff);
	pml4 = mxv_ept_page(mxv_vms[vm_index].eptp & MXV_EPT_ADDR_MASK);
	if (pml4 == NULL)
		return EFAULT;
	entry = pml4[pml4i];
	if (!(entry & MXV_EPT_PERM_MASK)) {
		r = mxv_ept_alloc_page(vm_index, &physical);
		if (r != OK)
			return r;
		pml4[pml4i] = physical | MXV_EPT_PERM_MASK;
		entry = pml4[pml4i];
	}
	pdpt = mxv_ept_page(entry & MXV_EPT_ADDR_MASK);
	if (pdpt == NULL)
		return EFAULT;
	entry = pdpt[pdpti];
	if (!(entry & MXV_EPT_PERM_MASK)) {
		r = mxv_ept_alloc_page(vm_index, &physical);
		if (r != OK)
			return r;
		pdpt[pdpti] = physical | MXV_EPT_PERM_MASK;
		entry = pdpt[pdpti];
	}
	/* 1-GiB leaves are never constructed by MXV. */
	if (entry & (1ULL << 7))
		return EEXIST;
	pd = mxv_ept_page(entry & MXV_EPT_ADDR_MASK);
	if (pd == NULL)
		return EFAULT;
	*pd_out = pd;
	return OK;
}

static int
mxv_ept_map_4k(unsigned int vm_index, u64_t gpa, u64_t hpa, u32_t flags)
{
	u64_t *pd, *pt, physical;
	unsigned int pdi = (unsigned int)((gpa >> 21) & 0x1ff);
	unsigned int pti = (unsigned int)((gpa >> 12) & 0x1ff);
	int r;

	r = mxv_ept_get_pd(vm_index, gpa, &pd);
	if (r != OK)
		return r;
	if (pd[pdi] == 0) {
		r = mxv_ept_alloc_page(vm_index, &physical);
		if (r != OK)
			return r;
		pd[pdi] = physical | MXV_EPT_PERM_MASK;
	} else if (pd[pdi] & (1ULL << 7)) {
		return EEXIST;
	}
	pt = mxv_ept_page(pd[pdi] & MXV_EPT_ADDR_MASK);
	if (pt == NULL)
		return EFAULT;
	if (pt[pti] != 0)
		return EEXIST;
	pt[pti] = (hpa & MXV_EPT_ADDR_MASK) |
	    (u64_t)(flags & MXV_EPT_PERM_MASK) | MXV_EPT_WB;
	return OK;
}

static int
mxv_ept_map_range(unsigned int vm_index, u64_t gpa, u64_t hpa,
	u64_t size, u32_t flags)
{
	u64_t offset;
	int r;

	for (offset = 0; offset < size; offset += MXV_PAGE_SIZE) {
		r = mxv_ept_map_4k(vm_index, gpa + offset,
		    hpa + offset, flags);
		if (r != OK)
			return r;
	}
	return OK;
}

/*
 * EPT trees are rebuilt only while no guest is executing them.  This is
 * Phase-2 metadata preparation; Phase 3 must add vCPU quiesce + INVEPT before
 * changing a live EPTP.
 */
static int
mxv_ept_rebuild(unsigned int vm_index)
{
	struct mxv_vm *vm = &mxv_vms[vm_index];
	u64_t root;
	unsigned int i;
	int r;

	mxv_ept_free_pages(vm_index);
	r = mxv_ept_alloc_page(vm_index, &root);
	if (r != OK)
		goto fail;
	/* EPTP: WB memory type and four-level page walk (encoded as 3). */
	vm->eptp = root | 6ULL | (3ULL << 3);

	for (i = 0; i < MXV_MAX_MEMSLOTS; i++) {
		struct mxv_memslot *slot = &vm->slots[i];
		struct mxv_pin *pin;

		if (!slot->in_use)
			continue;
		pin = mxv_pin_lookup(vm->owner, slot->pin_token);
		if (pin == NULL) {
			r = ESTALE;
			goto fail;
		}
		r = mxv_ept_map_range(vm_index, slot->gpa, pin->hpa,
		    slot->size, slot->flags);
		if (r != OK)
			goto fail;
	}
	return OK;

fail:
	mxv_ept_free_pages(vm_index);
	return r;
}

int
mxv_pin_range(endpoint_t owner, vir_bytes user_virt, u64_t size,
	u64_t *token)
{
	struct proc *target;
	struct mxv_pin *pin = NULL;
	phys_bytes first_hpa;
	u64_t end;
	unsigned int i, page, pin_index = 0;
	int proc_nr, r;

	if (token == NULL || size == 0 || size > MXV_MAX_TOTAL_RAM_BYTES ||
	    (user_virt & (MXV_PAGE_SIZE - 1)) != 0 ||
	    (size & (MXV_PAGE_SIZE - 1)) != 0)
		return EINVAL;
	if ((u64_t)user_virt + size < (u64_t)user_virt ||
	    (u64_t)user_virt + size > 0x0000800000000000ULL)
		return EINVAL;
	if (!mxv_endpoint_live(owner) || !isokendpt(owner, &proc_nr))
		return ESRCH;
	target = proc_addr(proc_nr);
	if (!HASPT(target))
		return EFAULT;

	/* Demand that the entire range is resident and physically contiguous. */
	if (vm_lookup_range(target, user_virt, &first_hpa, (size_t)size) != size)
		return EFAULT;
	if ((first_hpa & (MXV_PAGE_SIZE - 1)) != 0 ||
	    first_hpa + size < first_hpa ||
	    first_hpa + size > (1ULL << 52))
		return EINVAL;

	/* Every page must be a present, writable user mapping. */
	for (page = 0; page < size / MXV_PAGE_SIZE; page++) {
		phys_bytes this_hpa;
		vir_bytes this_virt = user_virt +
		    (vir_bytes)((u64_t)page * MXV_PAGE_SIZE);
		u32_t pte = 0;

		r = vm_lookup(target, this_virt, &this_hpa, &pte);
		if (r != OK ||
		    this_hpa != first_hpa +
			(phys_bytes)((u64_t)page * MXV_PAGE_SIZE) ||
		    !(pte & AMD64_VM_PRESENT) ||
		    !(pte & AMD64_VM_USER) ||
		    !(pte & AMD64_VM_WRITE))
			return EACCES;
	}

	end = (u64_t)first_hpa + size;
	spinlock_lock(&mxv_ept_lock);
	if (size > MXV_MAX_TOTAL_RAM_BYTES - mxv_pinned_bytes) {
		spinlock_unlock(&mxv_ept_lock);
		return ENOSPC;
	}
	for (i = 0; i < MXV_MAX_PINS; i++) {
		u64_t old_end;

		if (!mxv_pins[i].in_use) {
			if (pin == NULL) {
				pin = &mxv_pins[i];
				pin_index = i;
			}
			continue;
		}
		old_end = (u64_t)mxv_pins[i].hpa + mxv_pins[i].size;
		if ((u64_t)first_hpa < old_end &&
		    (u64_t)mxv_pins[i].hpa < end) {
			spinlock_unlock(&mxv_ept_lock);
			return EBUSY;
		}
	}
	if (pin == NULL) {
		spinlock_unlock(&mxv_ept_lock);
		return ENOSPC;
	}

	memset(pin, 0, sizeof(*pin));
	pin->in_use = 1;
	pin->owner = owner;
	pin->generation = mxv_generation();
	pin->hpa = first_hpa;
	pin->size = size;
	mxv_pinned_bytes += size;
	*token = mxv_handle(pin_index, pin->generation);
	spinlock_unlock(&mxv_ept_lock);
	return OK;
}

int
mxv_pin_release(endpoint_t owner, u64_t token)
{
	struct mxv_pin *pin;

	spinlock_lock(&mxv_ept_lock);
	pin = mxv_pin_lookup(owner, token);
	if (pin == NULL) {
		spinlock_unlock(&mxv_ept_lock);
		return ENOENT;
	}
	if (pin->attached) {
		spinlock_unlock(&mxv_ept_lock);
		return EBUSY;
	}
	if (mxv_pinned_bytes >= pin->size)
		mxv_pinned_bytes -= pin->size;
	else
		mxv_pinned_bytes = 0;
	memset(pin, 0, sizeof(*pin));
	spinlock_unlock(&mxv_ept_lock);
	return OK;
}

int
mxv_vm_create(endpoint_t owner, u64_t *handle)
{
	unsigned int i;
	struct mxv_vm *vm;

	if (handle == NULL || !mxv_endpoint_live(owner))
		return EINVAL;
	if ((mxv_vmx_capabilities() & MXV_CAP_REQUIRED) != MXV_CAP_REQUIRED)
		return ENOTSUP;
	spinlock_lock(&mxv_ept_lock);
	for (i = 0; i < MXV_MAX_VMS; i++)
		if (!mxv_vms[i].in_use)
			break;
	if (i == MXV_MAX_VMS) {
		spinlock_unlock(&mxv_ept_lock);
		return ENOSPC;
	}
	vm = &mxv_vms[i];
	memset(vm, 0, sizeof(*vm));
	vm->in_use = 1;
	vm->owner = owner;
	vm->generation = mxv_generation();
	*handle = mxv_handle(i, vm->generation);
	spinlock_unlock(&mxv_ept_lock);
	return OK;
}

int
mxv_vm_destroy(endpoint_t owner, u64_t handle)
{
	struct mxv_vm *vm;
	unsigned int i;

	spinlock_lock(&mxv_ept_lock);
	vm = mxv_vm_lookup(owner, handle, &i);
	if (vm == NULL) {
		spinlock_unlock(&mxv_ept_lock);
		return ENOENT;
	}
	for (i = 0; i < MXV_MAX_MEMSLOTS; i++) {
		if (vm->slots[i].in_use) {
			spinlock_unlock(&mxv_ept_lock);
			return EBUSY;
		}
	}
	mxv_ept_free_pages((unsigned int)(vm - mxv_vms));
	memset(vm, 0, sizeof(*vm));
	spinlock_unlock(&mxv_ept_lock);
	return OK;
}

int
mxv_mem_attach(endpoint_t owner, u64_t vm_handle, u64_t pin_token,
	u64_t gpa, u64_t size, u32_t flags, u32_t slot_id,
	u64_t *generation)
{
	struct mxv_vm *vm;
	struct mxv_pin *pin;
	struct mxv_memslot *slot;
	unsigned int vm_index, i;
	u64_t end;
	int r;

	if (generation == NULL || slot_id >= MXV_MAX_MEMSLOTS ||
	    size == 0 || size > MXV_MAX_TOTAL_RAM_BYTES ||
	    (gpa & (MXV_PAGE_SIZE - 1)) != 0 ||
	    (size & (MXV_PAGE_SIZE - 1)) != 0 ||
	    gpa + size < gpa || gpa + size > MXV_MAX_GPA ||
	    (flags & ~(MXV_MEM_READ | MXV_MEM_WRITE | MXV_MEM_EXEC |
		MXV_MEM_DIRTY_LOG)) != 0 ||
	    (flags & MXV_MEM_DIRTY_LOG) != 0 ||
	    !(flags & MXV_MEM_READ))
		return EINVAL;

	spinlock_lock(&mxv_ept_lock);
	vm = mxv_vm_lookup(owner, vm_handle, &vm_index);
	if (vm == NULL) {
		spinlock_unlock(&mxv_ept_lock);
		return ENOENT;
	}
	slot = &vm->slots[slot_id];
	if (slot->in_use) {
		spinlock_unlock(&mxv_ept_lock);
		return EEXIST;
	}
	pin = mxv_pin_lookup(owner, pin_token);
	if (pin == NULL || pin->attached || pin->size != size) {
		spinlock_unlock(&mxv_ept_lock);
		return EINVAL;
	}

	end = gpa + size;
	for (i = 0; i < MXV_MAX_MEMSLOTS; i++) {
		u64_t old_end;
		struct mxv_memslot *old = &vm->slots[i];

		if (!old->in_use)
			continue;
		old_end = old->gpa + old->size;
		if (gpa < old_end && old->gpa < end) {
			spinlock_unlock(&mxv_ept_lock);
			return EEXIST;
		}
	}

	memset(slot, 0, sizeof(*slot));
	slot->in_use = 1;
	slot->generation = mxv_generation();
	slot->flags = flags & (MXV_MEM_READ | MXV_MEM_WRITE | MXV_MEM_EXEC);
	slot->gpa = gpa;
	slot->size = size;
	slot->pin_token = pin_token;

	r = mxv_ept_rebuild(vm_index);
	if (r != OK) {
		memset(slot, 0, sizeof(*slot));
		(void)mxv_ept_rebuild(vm_index);
		spinlock_unlock(&mxv_ept_lock);
		return r;
	}
	pin->attached = 1;
	*generation = slot->generation;
	spinlock_unlock(&mxv_ept_lock);
	return OK;
}

int
mxv_mem_detach(endpoint_t owner, u64_t vm_handle, u32_t slot_id,
	u64_t generation)
{
	struct mxv_vm *vm;
	struct mxv_memslot saved;
	struct mxv_pin *pin;
	unsigned int vm_index;
	int r;

	if (slot_id >= MXV_MAX_MEMSLOTS)
		return EINVAL;
	spinlock_lock(&mxv_ept_lock);
	vm = mxv_vm_lookup(owner, vm_handle, &vm_index);
	if (vm == NULL) {
		spinlock_unlock(&mxv_ept_lock);
		return ENOENT;
	}
	if (!vm->slots[slot_id].in_use ||
	    vm->slots[slot_id].generation != (u32_t)generation) {
		spinlock_unlock(&mxv_ept_lock);
		return ESTALE;
	}

	saved = vm->slots[slot_id];
	memset(&vm->slots[slot_id], 0, sizeof(vm->slots[slot_id]));
	r = mxv_ept_rebuild(vm_index);
	if (r != OK) {
		vm->slots[slot_id] = saved;
		(void)mxv_ept_rebuild(vm_index);
		spinlock_unlock(&mxv_ept_lock);
		return r;
	}
	pin = mxv_pin_lookup(owner, saved.pin_token);
	if (pin != NULL)
		pin->attached = 0;
	spinlock_unlock(&mxv_ept_lock);
	return OK;
}

u64_t
mxv_vm_eptp(endpoint_t owner, u64_t vm_handle)
{
	struct mxv_vm *vm;
	u64_t eptp;

	spinlock_lock(&mxv_ept_lock);
	vm = mxv_vm_lookup(owner, vm_handle, NULL);
	eptp = vm == NULL ? 0 : vm->eptp;
	spinlock_unlock(&mxv_ept_lock);
	return eptp;
}

/*
 * Called before the owner's address space can be released.  Phase 2 EPTPs
 * are not active in a VMCS yet; Phase 3 must replace this with a vCPU stop,
 * I/O drain, INVEPT, and only then page unpin operation.
 */
void
mxv_owner_died(endpoint_t owner)
{
	unsigned int i;
	int proc_nr, vm_server_died;

	vm_server_died = isokendpt(owner, &proc_nr) &&
	    proc_nr == VM_PROC_NR;
	spinlock_lock(&mxv_ept_lock);
	if (vm_server_died) {
		/* VM owns the pin/region metadata; its death invalidates all of it. */
		for (i = 0; i < MXV_MAX_VMS; i++) {
			mxv_ept_free_pages(i);
			memset(&mxv_vms[i], 0, sizeof(mxv_vms[i]));
		}
		memset(mxv_pins, 0, sizeof(mxv_pins));
		mxv_pinned_bytes = 0;
		spinlock_unlock(&mxv_ept_lock);
		return;
	}
	for (i = 0; i < MXV_MAX_VMS; i++) {
		if (mxv_vms[i].in_use && mxv_vms[i].owner == owner) {
			mxv_ept_free_pages(i);
			memset(&mxv_vms[i], 0, sizeof(mxv_vms[i]));
		}
	}
	for (i = 0; i < MXV_MAX_PINS; i++) {
		if (mxv_pins[i].in_use && mxv_pins[i].owner == owner) {
			if (mxv_pinned_bytes >= mxv_pins[i].size)
				mxv_pinned_bytes -= mxv_pins[i].size;
			else
				mxv_pinned_bytes = 0;
			memset(&mxv_pins[i], 0, sizeof(mxv_pins[i]));
		}
	}
	spinlock_unlock(&mxv_ept_lock);
}
