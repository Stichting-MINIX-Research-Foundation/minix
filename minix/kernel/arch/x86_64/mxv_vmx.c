/*
 * MXV Phase-1 Intel VMX skeleton for MINIX/amd64.
 *
 * This file deliberately implements only:
 *   - fail-closed Intel VT-x/EPT capability discovery;
 *   - per-logical-CPU VMXON/VMXOFF;
 *   - bounded VMCS allocation and clear/load lifecycle;
 *   - one kernel-owned VMCALL probe to validate VMLAUNCH/VM-exit plumbing.
 *
 * The probe is NOT a guest OS and is NOT an isolation boundary.  It reuses
 * the current host CR3 and runs only the fixed instruction sequence in
 * mxv_entry.S.  No caller-provided guest code, RAM, register state, GPA, or
 * HPA is accepted.  EPT mappings and real guest memory are Phase 2.
 *
 * The fixed VMCS pool is temporary: the current kernel has no general
 * runtime page allocator suitable for long-lived VMX regions.  The pool is
 * bounded and allocation fails with ENOSPC instead of reusing live state.
 */
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <minix/mxvctl.h>

#include "kernel/kernel.h"
#include "arch_proto.h"
#include "archconst.h"

#define MXV_PAGE_SIZE                   4096
#define MXV_MAX_VCPUS                   8

#define IA32_FEATURE_CONTROL            0x0000003a
#define IA32_VMX_BASIC                  0x00000480
#define IA32_VMX_PINBASED_CTLS          0x00000481
#define IA32_VMX_PROCBASED_CTLS         0x00000482
#define IA32_VMX_EXIT_CTLS              0x00000483
#define IA32_VMX_ENTRY_CTLS             0x00000484
#define IA32_VMX_PROCBASED_CTLS2        0x0000048b
#define IA32_VMX_EPT_VPID_CAP           0x0000048c
#define IA32_VMX_TRUE_PINBASED_CTLS     0x0000048d
#define IA32_VMX_TRUE_PROCBASED_CTLS    0x0000048e
#define IA32_VMX_TRUE_EXIT_CTLS         0x0000048f
#define IA32_VMX_TRUE_ENTRY_CTLS        0x00000490
#define IA32_VMX_CR0_FIXED0             0x00000486
#define IA32_VMX_CR0_FIXED1             0x00000487
#define IA32_VMX_CR4_FIXED0             0x00000488
#define IA32_VMX_CR4_FIXED1             0x00000489
#define IA32_SYSENTER_CS                0x00000174
#define IA32_SYSENTER_ESP               0x00000175
#define IA32_SYSENTER_EIP               0x00000176
#define IA32_FS_BASE                    0xc0000100
#define IA32_GS_BASE                    0xc0000101
#define IA32_EFER                       0xc0000080

#define VMX_FEATURE_CONTROL_LOCK        (1ULL << 0)
#define VMX_FEATURE_CONTROL_VMX_OUTSIDE (1ULL << 2)
#define VMX_BASIC_TRUE_CTLS             (1ULL << 55)
#define VMX_BASIC_32BIT_PA              (1ULL << 48)
#define VMX_BASIC_MEMTYPE_MASK          (0xfULL << 50)
#define VMX_BASIC_MEMTYPE_WB            (6ULL << 50)
#define VMX_BASIC_REVISION_MASK         0x7fffffffU
#define VMX_BASIC_SIZE_SHIFT            32
#define VMX_BASIC_SIZE_MASK             0x1fffU

#define VMX_CR4_VMXE                    (1ULL << 13)
#define VMX_PIN_EXT_INTR_EXITING        (1U << 0)
#define VMX_CPU_HLT_EXITING             (1U << 7)
#define VMX_CPU_ACTIVATE_SECONDARY       (1U << 31)
#define VMX_EXIT_SAVE_DEBUG             (1U << 2)
#define VMX_EXIT_HOST_64BIT             (1U << 9)
#define VMX_EXIT_LOAD_EFER              (1U << 21)
#define VMX_ENTRY_LOAD_DEBUG            (1U << 2)
#define VMX_ENTRY_GUEST_IA32E           (1U << 9)
#define VMX_ENTRY_LOAD_EFER             (1U << 15)
#define VMX_EPT_CAP_4LEVEL              (1ULL << 6)
#define VMX_EPT_CAP_WB                  (1ULL << 14)
#define VMX_EPT_CAP_INVEPT              (1ULL << 20)
#define VMX_EPT_CAP_INVEPT_SINGLE       (1ULL << 25)

#define VMCS_CTRL_PIN_BASED             0x4000
#define VMCS_CTRL_PROC_BASED            0x4002
#define VMCS_CTRL_EXCEPTION_BITMAP      0x4004
#define VMCS_CTRL_CR3_TARGET_COUNT      0x400a
#define VMCS_CTRL_EXIT                  0x400c
#define VMCS_CTRL_ENTRY                 0x4012

#define VMCS_HOST_ES_SELECTOR           0x0c00
#define VMCS_HOST_CS_SELECTOR           0x0c02
#define VMCS_HOST_SS_SELECTOR           0x0c04
#define VMCS_HOST_DS_SELECTOR           0x0c06
#define VMCS_HOST_FS_SELECTOR           0x0c08
#define VMCS_HOST_GS_SELECTOR           0x0c0a
#define VMCS_HOST_TR_SELECTOR           0x0c0c
#define VMCS_HOST_IA32_SYSENTER_CS      0x4c00
#define VMCS_HOST_CR0                   0x6c00
#define VMCS_HOST_CR3                   0x6c02
#define VMCS_HOST_CR4                   0x6c04
#define VMCS_HOST_FS_BASE               0x6c06
#define VMCS_HOST_GS_BASE               0x6c08
#define VMCS_HOST_TR_BASE               0x6c0a
#define VMCS_HOST_GDTR_BASE             0x6c0c
#define VMCS_HOST_IDTR_BASE             0x6c0e
#define VMCS_HOST_IA32_SYSENTER_ESP     0x6c10
#define VMCS_HOST_IA32_SYSENTER_EIP     0x6c12
#define VMCS_HOST_RSP                   0x6c14
#define VMCS_HOST_RIP                   0x6c16
#define VMCS_HOST_IA32_EFER             0x2c02

#define VMCS_GUEST_ES_SELECTOR          0x0800
#define VMCS_GUEST_CS_SELECTOR          0x0802
#define VMCS_GUEST_SS_SELECTOR          0x0804
#define VMCS_GUEST_DS_SELECTOR          0x0806
#define VMCS_GUEST_FS_SELECTOR          0x0808
#define VMCS_GUEST_GS_SELECTOR          0x080a
#define VMCS_GUEST_LDTR_SELECTOR        0x080c
#define VMCS_GUEST_TR_SELECTOR          0x080e
#define VMCS_GUEST_ES_LIMIT             0x4800
#define VMCS_GUEST_CS_LIMIT             0x4802
#define VMCS_GUEST_SS_LIMIT             0x4804
#define VMCS_GUEST_DS_LIMIT             0x4806
#define VMCS_GUEST_FS_LIMIT             0x4808
#define VMCS_GUEST_GS_LIMIT             0x480a
#define VMCS_GUEST_LDTR_LIMIT           0x480c
#define VMCS_GUEST_TR_LIMIT             0x480e
#define VMCS_GUEST_GDTR_LIMIT           0x4810
#define VMCS_GUEST_IDTR_LIMIT           0x4812
#define VMCS_GUEST_ES_AR                0x4814
#define VMCS_GUEST_CS_AR                0x4816
#define VMCS_GUEST_SS_AR                0x4818
#define VMCS_GUEST_DS_AR                0x481a
#define VMCS_GUEST_FS_AR                0x481c
#define VMCS_GUEST_GS_AR                0x481e
#define VMCS_GUEST_LDTR_AR              0x4820
#define VMCS_GUEST_TR_AR                0x4822
#define VMCS_GUEST_INTERRUPTIBILITY    0x4824
#define VMCS_GUEST_ACTIVITY_STATE       0x4826
#define VMCS_GUEST_SYSENTER_CS          0x482a
#define VMCS_GUEST_CR0                  0x6800
#define VMCS_GUEST_CR3                  0x6802
#define VMCS_GUEST_CR4                  0x6804
#define VMCS_GUEST_ES_BASE              0x6806
#define VMCS_GUEST_CS_BASE              0x6808
#define VMCS_GUEST_SS_BASE              0x680a
#define VMCS_GUEST_DS_BASE              0x680c
#define VMCS_GUEST_FS_BASE              0x680e
#define VMCS_GUEST_GS_BASE              0x6810
#define VMCS_GUEST_LDTR_BASE            0x6812
#define VMCS_GUEST_TR_BASE              0x6814
#define VMCS_GUEST_GDTR_BASE            0x6816
#define VMCS_GUEST_IDTR_BASE            0x6818
#define VMCS_GUEST_DR7                  0x681a
#define VMCS_GUEST_RSP                  0x681c
#define VMCS_GUEST_RIP                  0x681e
#define VMCS_GUEST_RFLAGS               0x6820
#define VMCS_GUEST_PENDING_DEBUG        0x6822
#define VMCS_GUEST_SYSENTER_ESP         0x6824
#define VMCS_GUEST_SYSENTER_EIP         0x6826
#define VMCS_GUEST_VMCS_LINK_POINTER    0x2800
#define VMCS_GUEST_IA32_EFER            0x2806
#define VMCS_EXIT_REASON                0x4402
#define VMCS_EXIT_QUALIFICATION         0x6400
#define VMCS_VM_INSTRUCTION_ERROR       0x4400

struct mxv_desc_table {
	u16_t limit;
	u64_t base;
} __attribute__((packed));

struct mxv_segment_state {
	u64_t base;
	u32_t limit;
	u32_t access;
};

struct mxv_cpu_state {
	int vmxon;
	u64_t original_cr4;
	u64_t vmx_basic;
};

struct mxv_vcpu {
	int in_use;
	endpoint_t owner;
	unsigned cpu;
	u32_t generation;
	u32_t last_exit;
	u32_t last_error;
	u64_t last_qualification;
	u64_t last_rip;
};

static u8_t mxv_vmxon_regions[CONFIG_MAX_CPUS][MXV_PAGE_SIZE]
	__aligned(MXV_PAGE_SIZE);
static u8_t mxv_vmcs_regions[MXV_MAX_VCPUS][MXV_PAGE_SIZE]
	__aligned(MXV_PAGE_SIZE);
static struct mxv_cpu_state mxv_cpus[CONFIG_MAX_CPUS];
static struct mxv_vcpu mxv_vcpus[MXV_MAX_VCPUS];
static u32_t mxv_next_generation = 1;

extern int mxv_vmx_enter(struct mxv_vcpu *);
extern void mxv_vmx_probe_guest(void);

static u64_t mxv_rdmsr(u32_t msr)
{
	u32_t hi, lo;

	ia32_msr_read(msr, &hi, &lo);
	return ((u64_t)hi << 32) | lo;
}

static void mxv_cpuid(u32_t leaf, u32_t *a, u32_t *b,
	u32_t *c, u32_t *d)
{
	__asm__ volatile("cpuid"
		: "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
		: "a"(leaf), "c"(0)
		: "memory");
}

/* Return zero unless the current logical CPU is an Intel VMX CPU. */
u64_t mxv_vmx_capabilities(void)
{
	u32_t maxleaf, ebx, ecx, edx, eax;
	u64_t caps = 0, feature, basic, primary, secondary, ept;

	mxv_cpuid(0, &maxleaf, &ebx, &ecx, &edx);
	if (maxleaf < 1 || ebx != INTEL_CPUID_GEN_EBX ||
	    edx != INTEL_CPUID_GEN_EDX || ecx != INTEL_CPUID_GEN_ECX)
		return 0;

	mxv_cpuid(1, &eax, &ebx, &ecx, &edx);
	if (!(ecx & (1U << 5)))
		return 0;
	caps |= MXV_CAP_VMX;

	feature = mxv_rdmsr(IA32_FEATURE_CONTROL);
	if (feature & VMX_FEATURE_CONTROL_LOCK)
		caps |= MXV_CAP_FEATURE_LOCKED;
	if (feature & VMX_FEATURE_CONTROL_VMX_OUTSIDE)
		caps |= MXV_CAP_VMX_OUTSIDE_SMX;

	basic = mxv_rdmsr(IA32_VMX_BASIC);
	if (((basic >> VMX_BASIC_SIZE_SHIFT) & VMX_BASIC_SIZE_MASK) != 0 &&
	    ((basic >> VMX_BASIC_SIZE_SHIFT) & VMX_BASIC_SIZE_MASK) <= MXV_PAGE_SIZE &&
	    (basic & VMX_BASIC_MEMTYPE_MASK) == VMX_BASIC_MEMTYPE_WB)
		caps |= MXV_CAP_VMCS_WB;

	primary = mxv_rdmsr(IA32_VMX_PROCBASED_CTLS);
	if ((u32_t)(primary >> 32) & VMX_CPU_ACTIVATE_SECONDARY) {
		caps |= MXV_CAP_SECONDARY;
		secondary = mxv_rdmsr(IA32_VMX_PROCBASED_CTLS2);
		if ((u32_t)(secondary >> 32) & (1U << 1))
			caps |= MXV_CAP_EPT;
		if ((u32_t)(secondary >> 32) & (1U << 7))
			caps |= MXV_CAP_UNRESTRICTED;

		if (caps & MXV_CAP_EPT) {
			ept = mxv_rdmsr(IA32_VMX_EPT_VPID_CAP);
			if (ept & VMX_EPT_CAP_WB)
				caps |= MXV_CAP_EPT_WB;
			if (ept & VMX_EPT_CAP_4LEVEL)
				caps |= MXV_CAP_EPT_4LEVEL;
			if ((ept & VMX_EPT_CAP_INVEPT) &&
			    (ept & VMX_EPT_CAP_INVEPT_SINGLE))
				caps |= MXV_CAP_INVEPT;
		}
	}

	return caps;
}

static int mxv_region_pa(void *region, u64_t *pa)
{
	u64_t basic;

	if (((vir_bytes)region & (MXV_PAGE_SIZE - 1)) != 0)
		return EINVAL;

	*pa = vir2phys(region);
	if (*pa & (MXV_PAGE_SIZE - 1))
		return EINVAL;

	basic = mxv_rdmsr(IA32_VMX_BASIC);
	if ((basic & VMX_BASIC_32BIT_PA) && (*pa >> 32))
		return ENOTSUP;

	return OK;
}

static int mxv_vmxon(u64_t pa)
{
	unsigned char failed;

	__asm__ volatile("vmxon %1; setna %0"
		: "=qm"(failed)
		: "m"(pa)
		: "cc", "memory");

	return failed ? EIO : OK;
}

static int mxv_vmclear(u64_t pa)
{
	unsigned char failed;

	__asm__ volatile("vmclear %1; setna %0"
		: "=qm"(failed)
		: "m"(pa)
		: "cc", "memory");

	return failed ? EIO : OK;
}

static int mxv_vmptrld(u64_t pa)
{
	unsigned char failed;

	__asm__ volatile("vmptrld %1; setna %0"
		: "=qm"(failed)
		: "m"(pa)
		: "cc", "memory");

	return failed ? EIO : OK;
}

static int mxv_vmwrite(u64_t field, u64_t value)
{
	unsigned char failed;

	__asm__ volatile("vmwrite %2, %1; setna %0"
		: "=&q"(failed)
		: "r"(field), "rm"(value)
		: "cc");

	return failed ? EIO : OK;
}

static int mxv_vmread(u64_t field, u64_t *value)
{
	unsigned char failed;
	u64_t result;

	__asm__ volatile("vmread %2, %1; setna %0"
		: "=&q"(failed), "=r"(result)
		: "r"(field)
		: "cc");

	if (failed)
		return EIO;
	*value = result;
	return OK;
}

static int mxv_adjust_controls(u32_t msr, u32_t requested, u32_t *value)
{
	u64_t capability = mxv_rdmsr(msr);
	u32_t must_be_one = (u32_t)capability;
	u32_t may_be_one = (u32_t)(capability >> 32);

	if (must_be_one & ~may_be_one)
		return ENOTSUP;
	if (requested & ~may_be_one)
		return ENOTSUP;

	*value = (requested | must_be_one) & may_be_one;
	return OK;
}

static int mxv_vmx_control_msrs(struct mxv_cpu_state *cpu,
	u32_t *pin, u32_t *proc, u32_t *exit, u32_t *entry)
{
	u32_t pin_msr = IA32_VMX_PINBASED_CTLS;
	u32_t proc_msr = IA32_VMX_PROCBASED_CTLS;
	u32_t exit_msr = IA32_VMX_EXIT_CTLS;
	u32_t entry_msr = IA32_VMX_ENTRY_CTLS;
	int r;

	if (cpu->vmx_basic & VMX_BASIC_TRUE_CTLS) {
		pin_msr = IA32_VMX_TRUE_PINBASED_CTLS;
		proc_msr = IA32_VMX_TRUE_PROCBASED_CTLS;
		exit_msr = IA32_VMX_TRUE_EXIT_CTLS;
		entry_msr = IA32_VMX_TRUE_ENTRY_CTLS;
	}

	r = mxv_adjust_controls(pin_msr, VMX_PIN_EXT_INTR_EXITING, pin);
	if (r != OK)
		return r;
	r = mxv_adjust_controls(proc_msr, VMX_CPU_HLT_EXITING, proc);
	if (r != OK)
		return r;
	r = mxv_adjust_controls(exit_msr,
		VMX_EXIT_SAVE_DEBUG | VMX_EXIT_HOST_64BIT | VMX_EXIT_LOAD_EFER,
		exit);
	if (r != OK)
		return r;
	return mxv_adjust_controls(entry_msr,
		VMX_ENTRY_LOAD_DEBUG | VMX_ENTRY_GUEST_IA32E |
		VMX_ENTRY_LOAD_EFER, entry);
}

/* Decode a GDT descriptor into the VMCS segment format. */
static int mxv_segment(u64_t gdt_base, u16_t gdt_limit, u16_t selector,
	struct mxv_segment_state *segment)
{
	const struct segdesc_s *desc;
	u32_t index, limit;
	u64_t base;
	u32_t access;

	memset(segment, 0, sizeof(*segment));
	if (selector == 0) {
		segment->access = (1U << 16); /* unusable */
		return OK;
	}
	if (selector & 0x4)
		return ENOTSUP; /* LDT selectors are not used by this probe */

	index = selector & ~7U;
	if (index + sizeof(*desc) - 1 > gdt_limit)
		return EINVAL;

	desc = (const struct segdesc_s *)(vir_bytes)(gdt_base + index);
	base = (u64_t)desc->base_low |
		((u64_t)desc->base_middle << 16) |
		((u64_t)desc->base_high << 24);
	limit = (u32_t)desc->limit_low |
		((u32_t)(desc->granularity & 0x0f) << 16);
	access = (u32_t)desc->access |
		((u32_t)(desc->granularity & 0xf0) << 8);

	if (!(desc->access & 0x10)) {
		u32_t type = desc->access & 0x0f;

		/* A 64-bit TSS descriptor occupies two GDT entries. */
		if (type == 9 || type == 11) {
			u32_t upper;

			if (index + 15 > gdt_limit)
				return EINVAL;
			upper = *(const u32_t *)((const u8_t *)desc + 8);
			base |= (u64_t)upper << 32;
		}
	}
	if (desc->granularity & 0x80)
		limit = (limit << 12) | 0xfff;

	segment->base = base;
	segment->limit = limit;
	segment->access = access;
	return OK;
}

#define MXV_W(field, value) do { \
	if (mxv_vmwrite((field), (u64_t)(value)) != OK) \
		return EIO; \
} while (0)

int mxv_vmx_prepare_entry(struct mxv_vcpu *vcpu, u64_t host_rsp,
	u64_t host_rip)
{
	struct mxv_desc_table gdtr, idtr;
	struct mxv_segment_state es, cs, ss, ds, fs, gs, tr;
	u16_t host_es, host_cs, host_ss, host_ds, host_fs, host_gs, host_tr;
	u64_t cr0, cr3, cr4, efer, fs_base, gs_base, tr_base;
	u32_t pin, proc, exit, entry;
	u64_t fixed0, fixed1;
	int r;

	if (vcpu == NULL || !vcpu->in_use || vcpu->cpu != cpuid)
		return EINVAL;

	__asm__ volatile("sgdt %0" : "=m"(gdtr));
	__asm__ volatile("sidt %0" : "=m"(idtr));
	__asm__ volatile("mov %%es,%0" : "=r"(host_es));
	__asm__ volatile("mov %%cs,%0" : "=r"(host_cs));
	__asm__ volatile("mov %%ss,%0" : "=r"(host_ss));
	__asm__ volatile("mov %%ds,%0" : "=r"(host_ds));
	__asm__ volatile("mov %%fs,%0" : "=r"(host_fs));
	__asm__ volatile("mov %%gs,%0" : "=r"(host_gs));
	__asm__ volatile("str %0" : "=r"(host_tr));

	if ((host_cs & 3) != 0 || (host_ss & 3) != 0 || host_tr == 0)
		return EINVAL;

	r = mxv_segment(gdtr.base, gdtr.limit, host_es, &es);
	if (r != OK) return r;
	r = mxv_segment(gdtr.base, gdtr.limit, host_cs, &cs);
	if (r != OK) return r;
	r = mxv_segment(gdtr.base, gdtr.limit, host_ss, &ss);
	if (r != OK) return r;
	r = mxv_segment(gdtr.base, gdtr.limit, host_ds, &ds);
	if (r != OK) return r;
	r = mxv_segment(gdtr.base, gdtr.limit, host_fs, &fs);
	if (r != OK) return r;
	r = mxv_segment(gdtr.base, gdtr.limit, host_gs, &gs);
	if (r != OK) return r;
	r = mxv_segment(gdtr.base, gdtr.limit, host_tr, &tr);
	if (r != OK) return r;

	cr0 = read_cr0();
	cr3 = read_cr3();
	cr4 = read_cr4();
	efer = mxv_rdmsr(IA32_EFER);
	fixed0 = mxv_rdmsr(IA32_VMX_CR0_FIXED0);
	fixed1 = mxv_rdmsr(IA32_VMX_CR0_FIXED1);
	if ((cr0 & fixed0) != fixed0 || (cr0 & ~fixed1) != 0)
		return ENOTSUP;
	fixed0 = mxv_rdmsr(IA32_VMX_CR4_FIXED0);
	fixed1 = mxv_rdmsr(IA32_VMX_CR4_FIXED1);
	if ((cr4 & fixed0) != fixed0 || (cr4 & ~fixed1) != 0)
		return ENOTSUP;
	if (!(efer & (1ULL << 10))) /* LMA */
		return ENOTSUP;

	r = mxv_vmx_control_msrs(&mxv_cpus[cpuid], &pin, &proc,
		&exit, &entry);
	if (r != OK)
		return r;

	fs_base = mxv_rdmsr(IA32_FS_BASE);
	gs_base = mxv_rdmsr(IA32_GS_BASE);
	tr_base = tr.base;

	/* VM-execution controls: all exceptions and external interrupts exit. */
	MXV_W(VMCS_CTRL_PIN_BASED, pin);
	MXV_W(VMCS_CTRL_PROC_BASED, proc);
	MXV_W(VMCS_CTRL_EXCEPTION_BITMAP, 0xffffffffU);
	MXV_W(VMCS_CTRL_CR3_TARGET_COUNT, 0);
	MXV_W(VMCS_CTRL_EXIT, exit);
	MXV_W(VMCS_CTRL_ENTRY, entry);

	/* Host state restored by every VM exit. */
	MXV_W(VMCS_HOST_ES_SELECTOR, host_es & ~7U);
	MXV_W(VMCS_HOST_CS_SELECTOR, host_cs & ~7U);
	MXV_W(VMCS_HOST_SS_SELECTOR, host_ss & ~7U);
	MXV_W(VMCS_HOST_DS_SELECTOR, host_ds & ~7U);
	MXV_W(VMCS_HOST_FS_SELECTOR, host_fs & ~7U);
	MXV_W(VMCS_HOST_GS_SELECTOR, host_gs & ~7U);
	MXV_W(VMCS_HOST_TR_SELECTOR, host_tr & ~7U);
	MXV_W(VMCS_HOST_CR0, cr0);
	MXV_W(VMCS_HOST_CR3, cr3);
	MXV_W(VMCS_HOST_CR4, cr4);
	MXV_W(VMCS_HOST_FS_BASE, fs_base);
	MXV_W(VMCS_HOST_GS_BASE, gs_base);
	MXV_W(VMCS_HOST_TR_BASE, tr_base);
	MXV_W(VMCS_HOST_GDTR_BASE, gdtr.base);
	MXV_W(VMCS_HOST_IDTR_BASE, idtr.base);
	MXV_W(VMCS_HOST_IA32_SYSENTER_CS, mxv_rdmsr(IA32_SYSENTER_CS));
	MXV_W(VMCS_HOST_IA32_SYSENTER_ESP, mxv_rdmsr(IA32_SYSENTER_ESP));
	MXV_W(VMCS_HOST_IA32_SYSENTER_EIP, mxv_rdmsr(IA32_SYSENTER_EIP));
	MXV_W(VMCS_HOST_IA32_EFER, efer);
	MXV_W(VMCS_HOST_RSP, host_rsp);
	MXV_W(VMCS_HOST_RIP, host_rip);

	/* Guest state is a controlled CPL0 probe sharing the current host map. */
	MXV_W(VMCS_GUEST_ES_SELECTOR, host_es);
	MXV_W(VMCS_GUEST_CS_SELECTOR, host_cs);
	MXV_W(VMCS_GUEST_SS_SELECTOR, host_ss);
	MXV_W(VMCS_GUEST_DS_SELECTOR, host_ds);
	MXV_W(VMCS_GUEST_FS_SELECTOR, host_fs);
	MXV_W(VMCS_GUEST_GS_SELECTOR, host_gs);
	MXV_W(VMCS_GUEST_LDTR_SELECTOR, 0);
	MXV_W(VMCS_GUEST_TR_SELECTOR, host_tr);
	MXV_W(VMCS_GUEST_ES_LIMIT, es.limit);
	MXV_W(VMCS_GUEST_CS_LIMIT, cs.limit);
	MXV_W(VMCS_GUEST_SS_LIMIT, ss.limit);
	MXV_W(VMCS_GUEST_DS_LIMIT, ds.limit);
	MXV_W(VMCS_GUEST_FS_LIMIT, fs.limit);
	MXV_W(VMCS_GUEST_GS_LIMIT, gs.limit);
	MXV_W(VMCS_GUEST_LDTR_LIMIT, 0);
	MXV_W(VMCS_GUEST_TR_LIMIT, tr.limit);
	MXV_W(VMCS_GUEST_GDTR_LIMIT, gdtr.limit);
	MXV_W(VMCS_GUEST_IDTR_LIMIT, idtr.limit);
	MXV_W(VMCS_GUEST_ES_AR, es.access);
	MXV_W(VMCS_GUEST_CS_AR, cs.access);
	MXV_W(VMCS_GUEST_SS_AR, ss.access);
	MXV_W(VMCS_GUEST_DS_AR, ds.access);
	MXV_W(VMCS_GUEST_FS_AR, fs.access);
	MXV_W(VMCS_GUEST_GS_AR, gs.access);
	MXV_W(VMCS_GUEST_LDTR_AR, 1U << 16);
	MXV_W(VMCS_GUEST_TR_AR, tr.access);
	MXV_W(VMCS_GUEST_ES_BASE, es.base);
	MXV_W(VMCS_GUEST_CS_BASE, cs.base);
	MXV_W(VMCS_GUEST_SS_BASE, ss.base);
	MXV_W(VMCS_GUEST_DS_BASE, ds.base);
	MXV_W(VMCS_GUEST_FS_BASE, fs_base);
	MXV_W(VMCS_GUEST_GS_BASE, gs_base);
	MXV_W(VMCS_GUEST_LDTR_BASE, 0);
	MXV_W(VMCS_GUEST_TR_BASE, tr.base);
	MXV_W(VMCS_GUEST_GDTR_BASE, gdtr.base);
	MXV_W(VMCS_GUEST_IDTR_BASE, idtr.base);
	MXV_W(VMCS_GUEST_CR0, cr0);
	MXV_W(VMCS_GUEST_CR3, cr3);
	MXV_W(VMCS_GUEST_CR4, cr4);
	MXV_W(VMCS_GUEST_DR7, 0x400);
	MXV_W(VMCS_GUEST_RSP, 0);
	MXV_W(VMCS_GUEST_RIP, (vir_bytes)mxv_vmx_probe_guest);
	MXV_W(VMCS_GUEST_RFLAGS, 2);
	MXV_W(VMCS_GUEST_PENDING_DEBUG, 0);
	MXV_W(VMCS_GUEST_INTERRUPTIBILITY, 0);
	MXV_W(VMCS_GUEST_ACTIVITY_STATE, 0);
	MXV_W(VMCS_GUEST_SYSENTER_CS, mxv_rdmsr(IA32_SYSENTER_CS));
	MXV_W(VMCS_GUEST_SYSENTER_ESP, mxv_rdmsr(IA32_SYSENTER_ESP));
	MXV_W(VMCS_GUEST_SYSENTER_EIP, mxv_rdmsr(IA32_SYSENTER_EIP));
	MXV_W(VMCS_GUEST_VMCS_LINK_POINTER, ~(u64_t)0);
	MXV_W(VMCS_GUEST_IA32_EFER, efer);

	return OK;
}

#undef MXV_W

int mxv_vmx_exit_complete(struct mxv_vcpu *vcpu)
{
	u64_t reason, qualification, guest_rip;

	if (mxv_vmread(VMCS_EXIT_REASON, &reason) != OK)
		return EIO;
	if (mxv_vmread(VMCS_EXIT_QUALIFICATION, &qualification) != OK)
		return EIO;
	if (mxv_vmread(VMCS_GUEST_RIP, &guest_rip) != OK)
		return EIO;

	vcpu->last_exit = (u32_t)(reason & 0xffff);
	vcpu->last_qualification = qualification;
	vcpu->last_rip = guest_rip;
	vcpu->last_error = 0;
	if (vcpu->last_exit != MXV_EXIT_VMCALL)
		return EIO;

	return OK;
}

int mxv_vmx_entry_failed(struct mxv_vcpu *vcpu)
{
	u64_t error = 0;

	if (mxv_vmread(VMCS_VM_INSTRUCTION_ERROR, &error) == OK)
		vcpu->last_error = (u32_t)error;
	else
		vcpu->last_error = 0xffffffffU;
	return EIO;
}

static struct mxv_vcpu *mxv_vcpu_lookup(endpoint_t owner, u64_t handle)
{
	u32_t slot = (u32_t)handle;
	u32_t generation = (u32_t)(handle >> 32);
	struct mxv_vcpu *vcpu;

	if (slot == 0 || slot > MXV_MAX_VCPUS)
		return NULL;
	vcpu = &mxv_vcpus[slot - 1];
	if (!vcpu->in_use || vcpu->owner != owner ||
	    vcpu->generation != generation)
		return NULL;
	return vcpu;
}

int mxv_vmx_cpu_enable(void)
{
	struct mxv_cpu_state *cpu;
	u64_t caps, basic, pa;
	u32_t revision;
	u64_t cr4;
	unsigned char failed;
	int r;

	if (cpuid >= CONFIG_MAX_CPUS)
		return EINVAL;
	cpu = &mxv_cpus[cpuid];
	if (cpu->vmxon)
		return OK;

	caps = mxv_vmx_capabilities();
	if ((caps & MXV_CAP_REQUIRED) != MXV_CAP_REQUIRED)
		return ENOTSUP;

	basic = mxv_rdmsr(IA32_VMX_BASIC);
	r = mxv_region_pa(mxv_vmxon_regions[cpuid], &pa);
	if (r != OK)
		return r;
	revision = (u32_t)(basic & VMX_BASIC_REVISION_MASK);
	memset(mxv_vmxon_regions[cpuid], 0, MXV_PAGE_SIZE);
	*(u32_t *)mxv_vmxon_regions[cpuid] = revision;

	cpu->original_cr4 = read_cr4();
	cr4 = cpu->original_cr4 | VMX_CR4_VMXE;
	write_cr4(cr4);

	r = mxv_vmxon(pa);
	if (r != OK) {
		write_cr4(cpu->original_cr4);
		return r;
	}

	cpu->vmx_basic = basic;
	cpu->vmxon = 1;
	return OK;
}

int mxv_vmx_cpu_disable(void)
{
	struct mxv_cpu_state *cpu;
	int i;

	if (cpuid >= CONFIG_MAX_CPUS)
		return EINVAL;
	cpu = &mxv_cpus[cpuid];
	if (!cpu->vmxon)
		return OK;

	for (i = 0; i < MXV_MAX_VCPUS; i++) {
		if (mxv_vcpus[i].in_use && mxv_vcpus[i].cpu == cpuid)
			return EBUSY;
	}

	__asm__ volatile("vmxoff" ::: "cc", "memory");
	write_cr4(cpu->original_cr4);
	cpu->vmxon = 0;
	cpu->vmx_basic = 0;
	return OK;
}

int mxv_vcpu_create(endpoint_t owner, u64_t *handle)
{
	struct mxv_vcpu *vcpu;
	u64_t pa;
	u32_t revision;
	int i, r;

	if (handle == NULL || cpuid >= CONFIG_MAX_CPUS)
		return EINVAL;
	if (!mxv_cpus[cpuid].vmxon)
		return ENXIO;

	for (i = 0; i < MXV_MAX_VCPUS; i++)
		if (!mxv_vcpus[i].in_use)
			break;
	if (i == MXV_MAX_VCPUS)
		return ENOSPC;

	r = mxv_region_pa(mxv_vmcs_regions[i], &pa);
	if (r != OK)
		return r;
	revision = (u32_t)(mxv_cpus[cpuid].vmx_basic &
		VMX_BASIC_REVISION_MASK);
	memset(mxv_vmcs_regions[i], 0, MXV_PAGE_SIZE);
	*(u32_t *)mxv_vmcs_regions[i] = revision;

	r = mxv_vmclear(pa);
	if (r != OK)
		return r;

	vcpu = &mxv_vcpus[i];
	memset(vcpu, 0, sizeof(*vcpu));
	vcpu->in_use = 1;
	vcpu->owner = owner;
	vcpu->cpu = cpuid;
	vcpu->generation = mxv_next_generation++;
	if (vcpu->generation == 0)
		vcpu->generation = mxv_next_generation++;

	*handle = ((u64_t)vcpu->generation << 32) | (u32_t)(i + 1);
	return OK;
}

int mxv_vcpu_destroy(endpoint_t owner, u64_t handle)
{
	struct mxv_vcpu *vcpu;
	u64_t pa;
	int r;

	vcpu = mxv_vcpu_lookup(owner, handle);
	if (vcpu == NULL)
		return EINVAL;
	if (vcpu->cpu != cpuid)
		return EBUSY;

	r = mxv_region_pa(mxv_vmcs_regions[vcpu - mxv_vcpus], &pa);
	if (r != OK)
		return r;
	r = mxv_vmclear(pa);
	if (r != OK)
		return r;

	memset(vcpu, 0, sizeof(*vcpu));
	return OK;
}

int mxv_vcpu_run_probe(endpoint_t owner, u64_t handle, u64_t *exit_reason)
{
	struct mxv_vcpu *vcpu;
	u64_t pa;
	int r;

	if (exit_reason == NULL)
		return EINVAL;
	vcpu = mxv_vcpu_lookup(owner, handle);
	if (vcpu == NULL)
		return EINVAL;
	if (vcpu->cpu != cpuid)
		return EBUSY;
	if (!mxv_cpus[cpuid].vmxon)
		return ENXIO;

	r = mxv_region_pa(mxv_vmcs_regions[vcpu - mxv_vcpus], &pa);
	if (r != OK)
		return r;
	r = mxv_vmclear(pa);
	if (r != OK)
		return r;
	r = mxv_vmptrld(pa);
	if (r != OK)
		return r;

	vcpu->last_exit = 0;
	vcpu->last_error = 0;
	r = mxv_vmx_enter(vcpu);
	if (r != OK)
		return r;

	*exit_reason = vcpu->last_exit;
	return OK;
}
