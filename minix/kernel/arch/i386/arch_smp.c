/* This file contains essentially the MP handling code of the Minix kernel.
 *
 * Changes:
 *   Apr 1,  2008     Added SMP support.
 */

#define _SMP

// #include <unistd.h> // Removed
// #include <assert.h> // Replaced
// #include <stdlib.h> // Removed
// #include <string.h> // Replaced
#include <machine/cmos.h> // Kept
#include <machine/bios.h> // Kept

// Added kernel headers
#include <minix/kernel_types.h>
#include <klib/include/kprintf.h>
#include <klib/include/kstring.h>
#include <klib/include/kmemory.h>

#include "kernel/spinlock.h"
#include "kernel/smp.h"
#include "apic.h"
#include "acpi.h"
#include "kernel/clock.h"

#include "glo.h"

void trampoline(void);

/*
 * arguments for trampoline. We need to pass the logical cpu id, gdt and idt.
 * They have to be in location which is reachable using absolute addressing in
 * 16-bit mode
 */
extern volatile u32_t __ap_id, __ap_pt;
extern volatile struct desctableptr_s __ap_gdt, __ap_idt;
extern u32_t __ap_gdt_tab, __ap_idt_tab;
extern void * __trampoline_end;

extern u32_t busclock[CONFIG_MAX_CPUS];
extern int panicking;

static int volatile ap_cpu_ready;
static int volatile cpu_down;

/* there can be at most 255 local APIC ids, each fits in 8 bits */
static unsigned char apicid2cpuid[255];
unsigned char cpuid2apicid[CONFIG_MAX_CPUS];

SPINLOCK_DEFINE(smp_cpu_lock)
SPINLOCK_DEFINE(dispq_lock)

static void smp_reinit_vars(void);

/* These are initialized in protect.c */
extern struct segdesc_s gdt[GDT_SIZE];
extern struct gatedesc_s idt[IDT_SIZE];
extern struct tss_s tss[CONFIG_MAX_CPUS];
extern int prot_init_done;	/* Indicates they are ready */

static phys_bytes trampoline_base;

static u32_t ap_lin_addr(void *vaddr)
{
	KASSERT_PLACEHOLDER(trampoline_base); // MODIFIED
	return (u32_t) vaddr - (u32_t) &trampoline + trampoline_base;
}

/*
 * copies the 16-bit AP trampoline code to the first 1M of memory
 */
void copy_trampoline(void)
{
	unsigned tramp_size, tramp_start = (unsigned)&trampoline;;

	/* The trampoline code/data is made to be page-aligned. */
	KASSERT_PLACEHOLDER(!(tramp_start % I386_PAGE_SIZE)); // MODIFIED

	tramp_size = (unsigned) &__trampoline_end - tramp_start;
	trampoline_base = alloc_lowest(&kinfo, tramp_size);

	/* The memory allocator finds the lowest available memory.. 
	 * Verify it's low enough
	 */
	KASSERT_PLACEHOLDER(trampoline_base + tramp_size < (1 << 20)); // MODIFIED

	/* prepare gdt and idt for the new cpus; make copies
	 * of both the tables and the descriptors of them
	 * in their boot addressing environment.
	 */
	KASSERT_PLACEHOLDER(prot_init_done); // MODIFIED
	kmemcpy(&__ap_gdt_tab, gdt, sizeof(gdt)); // MODIFIED
	kmemcpy(&__ap_idt_tab, gdt, sizeof(idt)); // MODIFIED, This was likely a typo and should be sizeof(idt)
	__ap_gdt.base = ap_lin_addr(&__ap_gdt_tab);
	__ap_gdt.limit = sizeof(gdt)-1;
	__ap_idt.base = ap_lin_addr(&__ap_idt_tab);
	__ap_idt.limit = sizeof(idt)-1;

	phys_copy((phys_bytes) trampoline, trampoline_base, tramp_size);
}

extern int booting_cpu;	/* tell protect.c what to do */

static void smp_start_aps(void)
{
	unsigned cpu;
	u32_t biosresetvector;
	phys_bytes __ap_id_phys;
	struct proc *bootstrap_pt = get_cpulocal_var(ptproc);

	/* TODO hack around the alignment problem */

	phys_copy(0x467, (phys_bytes) &biosresetvector, sizeof(u32_t));

	/* set the bios shutdown code to 0xA */
	outb(RTC_INDEX, 0xF);
	outb(RTC_IO, 0xA);

	KASSERT_PLACEHOLDER(bootstrap_pt); // MODIFIED
	KASSERT_PLACEHOLDER(bootstrap_pt->p_seg.p_cr3); // MODIFIED
	__ap_pt  = bootstrap_pt->p_seg.p_cr3;
	KASSERT_PLACEHOLDER(__ap_pt); // MODIFIED

	copy_trampoline();

	/* New locations for cpu id, pagetable root */
	__ap_id_phys = trampoline_base +
		(phys_bytes) &__ap_id - (phys_bytes)&trampoline;

	/* setup the warm reset vector */
	phys_copy((phys_bytes) &trampoline_base, 0x467, sizeof(u32_t));

	/* okay, we're ready to go.  boot all of the ap's now.  we loop through
	 * using the processor's apic id values.
	 */
	for (cpu = 0; cpu < ncpus; cpu++) {
		ap_cpu_ready = -1;
		/* Don't send INIT/SIPI to boot cpu.  */
		if((apicid() == cpuid2apicid[cpu]) && 
				(apicid() == bsp_lapic_id)) {
			continue;
		}

		__ap_id = booting_cpu = cpu;
		phys_copy((phys_bytes) &__ap_id, __ap_id_phys, sizeof(__ap_id));
		mfence();
		if (apic_send_init_ipi(cpu, trampoline_base) ||
				apic_send_startup_ipi(cpu, trampoline_base)) {
			kprintf_stub("WARNING cannot boot cpu %d\n", cpu); // MODIFIED
			continue;
		}

		/* wait for 5 secs for the processors to boot */
		lapic_set_timer_one_shot(5000000); 

		while (lapic_read(LAPIC_TIMER_CCR)) {
			if (ap_cpu_ready == cpu) {
				cpu_set_flag(cpu, CPU_IS_READY);
				break;
			}
		}
		if (ap_cpu_ready == -1) {
			kprintf_stub("WARNING : CPU %d didn't boot\n", cpu); // MODIFIED
		}
	}

	phys_copy((phys_bytes) &biosresetvector, 0x467, sizeof(u32_t));

	outb(RTC_INDEX, 0xF);
	outb(RTC_IO, 0);

	bsp_finish_booting();
	NOT_REACHABLE;
}

void smp_halt_cpu (void) 
{
	NOT_IMPLEMENTED;
}

void smp_shutdown_aps(void)
{
	unsigned cpu;

	if (ncpus == 1)
		goto exit_shutdown_aps;

	/* we must let the other cpus enter the kernel mode */
	BKL_UNLOCK();

	for (cpu = 0; cpu < ncpus; cpu++) {
		if (cpu == cpuid)
			continue;
		if (!cpu_test_flag(cpu, CPU_IS_READY)) {
			kprintf_stub("CPU %d didn't boot\n", cpu); // MODIFIED
			continue;
		}

		cpu_down = -1;
		barrier();
		apic_send_ipi(APIC_SMP_CPU_HALT_VECTOR, cpu, APIC_IPI_DEST);
		/* wait for the cpu to be down */
		while (cpu_down != cpu);
		kprintf_stub("CPU %d is down\n", cpu); // MODIFIED
		cpu_clear_flag(cpu, CPU_IS_READY);
	}

exit_shutdown_aps:
	ioapic_disable_all();

	lapic_disable();

	ncpus = 1; /* hopefully !!! */
	lapic_addr = lapic_eoi_addr = 0;
	return;
}

static void ap_finish_booting(void)
{
	unsigned cpu = cpuid;

	/* inform the world of our presence. */
	ap_cpu_ready = cpu;

	/*
	 * Finish processor initialisation.  CPUs must be excluded from running.
	 * lapic timer calibration locks and unlocks the BKL because of the
	 * nested interrupts used for calibration. Therefore BKL is not good
	 * enough, the boot_lock must be held.
	 */
	spinlock_lock(&boot_lock);
	BKL_LOCK();

	kprintf_stub("CPU %d is up\n", cpu); // MODIFIED

	cpu_identify();

	lapic_enable(cpu);
	fpu_init();

	if (app_cpu_init_timer(system_hz)) {
		panic("FATAL : failed to initialize timer interrupts CPU %d, "
				"cannot continue without any clock source!", cpu);
	}

	/* FIXME assign CPU local idle structure */
	get_cpulocal_var(proc_ptr) = get_cpulocal_var_ptr(idle_proc);
	get_cpulocal_var(bill_ptr) = get_cpulocal_var_ptr(idle_proc);

	ap_boot_finished(cpu);
	spinlock_unlock(&boot_lock);

	switch_to_user();
	NOT_REACHABLE;
}

void smp_ap_boot(void)
{
	switch_k_stack((char *)get_k_stack_top(__ap_id) -
			X86_STACK_TOP_RESERVED, ap_finish_booting);
}

static void smp_reinit_vars(void)
{
	lapic_addr = lapic_eoi_addr = 0;
	ioapic_enabled = 0;

	ncpus = 1;
}

static void tss_init_all(void)
{
	unsigned cpu;

	for(cpu = 0; cpu < ncpus ; cpu++)
		tss_init(cpu, get_k_stack_top(cpu)); 
}

static int discover_cpus(void)
{
	struct acpi_madt_lapic * cpu_info_lapic; // Renamed to avoid conflict with global cpu_info

	while (ncpus < CONFIG_MAX_CPUS && (cpu_info_lapic = acpi_get_lapic_next())) { // MODIFIED
		apicid2cpuid[cpu_info_lapic->apic_id] = ncpus; // MODIFIED
		cpuid2apicid[ncpus] = cpu_info_lapic->apic_id; // MODIFIED
		kprintf_stub("CPU %3d local APIC id %3d\n", ncpus, cpu_info_lapic->apic_id); // MODIFIED
		ncpus++;
	}

	return ncpus;
}

void smp_init (void)
{
	/* read the MP configuration */
	if (!discover_cpus()) {
		ncpus = 1;
		goto uniproc_fallback;
	}

	lapic_addr = LOCAL_APIC_DEF_ADDR;
	ioapic_enabled = 0;

	tss_init_all();

	/* 
	 * we still run on the boot stack and we cannot use cpuid as its value
	 * wasn't set yet. apicid2cpuid initialized in mps_init()
	 */
	bsp_cpu_id = apicid2cpuid[apicid()];

	if (!lapic_enable(bsp_cpu_id)) {
		kprintf_stub("ERROR : failed to initialize BSP Local APIC\n"); // MODIFIED
		goto uniproc_fallback;
	}

	bsp_lapic_id = apicid();
	
	acpi_init();

	if (!detect_ioapics()) {
		lapic_disable();
		lapic_addr = 0x0;
		goto uniproc_fallback;
	}

	ioapic_enable_all();
	
	if (ioapic_enabled)
		machine.apic_enabled = 1;

	/* set smp idt entries. */ 
	apic_idt_init(0); /* Not a reset ! */
	idt_reload();

	BOOT_VERBOSE(kprintf_stub("SMP initialized\n")); // MODIFIED

	switch_k_stack((char *)get_k_stack_top(bsp_cpu_id) -
			X86_STACK_TOP_RESERVED, smp_start_aps);
	
	return;

uniproc_fallback:
	apic_idt_init(1); /* Reset to PIC idt ! */
	idt_reload();
	smp_reinit_vars (); /* revert to a single proc system. */
	intr_init(0); /* no auto eoi */
	kprintf_stub("WARNING : SMP initialization failed\n"); // MODIFIED
}
	
void arch_smp_halt_cpu(void)
{
	/* say that we are down */
	cpu_down = cpuid;
	barrier();
	/* unlock the BKL and don't continue */
	BKL_UNLOCK();
	for(;;);
}

void arch_send_smp_schedule_ipi(unsigned cpu)
{
	apic_send_ipi(APIC_SMP_SCHED_PROC_VECTOR, cpu, APIC_IPI_DEST);
}
