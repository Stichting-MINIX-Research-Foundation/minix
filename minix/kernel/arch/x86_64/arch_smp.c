/* This file contains essentially the MP handling code of the Minix kernel.
 *
 * Changes:
 *   Apr 1,  2008     Added SMP support.
 */

#define _SMP

#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <machine/cmos.h>
#include <machine/bios.h>

#include "kernel/kernel.h"
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
	assert(trampoline_base);
	return (u32_t) vaddr - (u32_t) &trampoline + trampoline_base;
}

/*
 * copies the 16-bit AP trampoline code to the first 1M of memory
 */
void copy_trampoline(void)
{
	unsigned tramp_size, tramp_start = (unsigned)&trampoline;;

	/* The trampoline code/data is made to be page-aligned. */
	assert(!(tramp_start % I386_PAGE_SIZE));

	tramp_size = (unsigned) &__trampoline_end - tramp_start;
	trampoline_base = alloc_lowest(&kinfo, tramp_size);

	/* The memory allocator finds the lowest available memory.. 
	 * Verify it's low enough
	 */
	assert(trampoline_base + tramp_size < (1 << 20));

	/* prepare gdt and idt for the new cpus; make copies
	 * of both the tables and the descriptors of them
	 * in their boot addressing environment.
	 */
	assert(prot_init_done);
	memcpy(&__ap_gdt_tab, gdt, sizeof(gdt));
	memcpy(&__ap_idt_tab, idt, sizeof(idt));
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

	assert(bootstrap_pt);
	assert(bootstrap_pt->p_seg.p_cr3);
	__ap_pt  = bootstrap_pt->p_seg.p_cr3;
	assert(__ap_pt);

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
		/* Don't send INIT/SIPI to the bootstrap processor. */
		if (cpu == bsp_cpu_id)
			continue;

		/* Publish STARTING before an AP can receive its startup IPI. */
		if (!cpu_transition(cpu, CPU_STATE_OFFLINE, CPU_STATE_STARTING)) {
			printf("WARNING CPU %u has invalid initial state %u\n",
					cpu, cpu_state(cpu));
			continue;
		}

		__ap_id = booting_cpu = cpu;
		phys_copy((phys_bytes) &__ap_id, __ap_id_phys, sizeof(__ap_id));
		mfence();
		if (apic_send_init_ipi(cpu, trampoline_base) ||
				apic_send_startup_ipi(cpu, trampoline_base)) {
			/*
			 * Do not overwrite a state already claimed by the AP. If it
			 * entered C concurrently with an IPI error, it owns the next
			 * transition and will either reach ONLINE or report FAILED.
			 */
			if (cpu_transition(cpu, CPU_STATE_STARTING,
			    CPU_STATE_FAILED)) {
				printf("WARNING cannot boot cpu %u\n", cpu);
				/*
				 * The low-memory trampoline has one shared CPU-ID
				 * slot. Do not overwrite it while a timed-out AP
				 * could still be executing the trampoline.
				 */
				break;
			}
		}

		/* Wait up to five seconds for STARTING -> INITIALIZING. */
		lapic_set_timer_one_shot(5000000);
		while (lapic_read(LAPIC_TIMER_CCR) &&
		    cpu_state(cpu) == CPU_STATE_STARTING)
			arch_pause();

		/*
		 * The timeout is conditional: if the AP won STARTING ->
		 * INITIALIZING, the failed transition below cannot affect it.
		 */
		if (cpu_transition(cpu, CPU_STATE_STARTING, CPU_STATE_FAILED)) {
			printf("WARNING CPU %u did not reach its startup entry\n",
					cpu);
			/*
			 * Keep the trampoline's CPU ID stable for a late AP. The
			 * remaining processors stay OFFLINE rather than risking
			 * that they boot with the wrong logical ID.
			 */
			break;
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

	if (smp_online_count() <= 1)
		goto exit_shutdown_aps;

	/* we must let the other cpus enter the kernel mode */
	BKL_UNLOCK();

	for (cpu = 0; cpu < ncpus; cpu++) {
		if (cpu == cpuid)
			continue;
		if (!cpu_transition(cpu, CPU_STATE_ONLINE,
		    CPU_STATE_STOPPING)) {
			printf("CPU %d didn't boot\n", cpu);
			continue;
		}
		cpu_down = -1;
		barrier();
		apic_send_ipi(APIC_SMP_CPU_HALT_VECTOR, cpu, APIC_IPI_DEST);
		/* wait for the cpu to be down */
		while (cpu_down != cpu);
		printf("CPU %d is down\n", cpu);
		if (!cpu_transition(cpu, CPU_STATE_STOPPING,
		    CPU_STATE_OFFLINE))
			panic("CPU %u has invalid shutdown state", cpu);
	}

exit_shutdown_aps:
	ioapic_disable_all();

	lapic_disable();

	ncpus = bsp_cpu_id + 1; /* retain the BSP's logical CPU ID */
	lapic_addr = lapic_eoi_addr = 0;
	return;
}

static void ap_finish_booting(void)
{
	unsigned cpu = cpuid;
	int lapic_ready = 0;

	/*
	 * Claim the startup state before touching per-CPU initialization. If
	 * the BSP already timed out this CPU, this CAS fails and the late AP
	 * cannot join the system.
	 */
	if (!cpu_transition(cpu, CPU_STATE_STARTING, CPU_STATE_INITIALIZING)) {
		intr_disable();
		for (;;)
			__asm__ volatile("hlt" ::: "memory");
	}

	/* Set up sysenter/syscall. */
	setup_sysenter_syscall();

	/*
	 * Finish processor initialisation.  CPUs must be excluded from running.
	 * lapic timer calibration locks and unlocks the BKL because of the
	 * nested interrupts used for calibration. Therefore BKL is not good
	 * enough, the boot_lock must be held.
	 */
	spinlock_lock(&boot_lock);
	BKL_LOCK();

	/* The BSP may have timed out initialization while this AP waited. */
	if (cpu_state(cpu) != CPU_STATE_INITIALIZING)
		goto failed;

	printf("CPU %d is up\n", cpu);

	cpu_identify();

	if (!lapic_enable(cpu)) {
		printf("WARNING : CPU %u failed to initialize its Local APIC\n",
				cpu);
		goto failed;
	}
	lapic_ready = 1;
	fpu_init();

	if (app_cpu_init_timer(system_hz)) {
		printf("WARNING : CPU %u failed to initialize its local timer\n",
				cpu);
		goto failed;
	}

	/* FIXME assign CPU local idle structure */
	get_cpulocal_var(proc_ptr) = get_cpulocal_var_ptr(idle_proc);
	get_cpulocal_var(bill_ptr) = get_cpulocal_var_ptr(idle_proc);

	/*
	 * ONLINE is a terminal state. If the BSP's initialization deadline won
	 * the CAS, discard this partially initialized AP instead of reviving it.
	 */
	if (!ap_boot_finished(cpu))
		goto failed;
	spinlock_unlock(&boot_lock);

	switch_to_user();
	NOT_REACHABLE;

failed:
	if (lapic_ready)
		stop_local_timer();
	intr_disable();
	ap_boot_failed(cpu);
	spinlock_unlock(&boot_lock);
	BKL_UNLOCK();
	for (;;)
		__asm__ volatile("hlt" ::: "memory");
}

void smp_ap_boot(void)
{
	switch_k_stack(X86_STACK_SWITCH_SP(__ap_id),
			ap_finish_booting);
}

static void smp_reinit_vars(void)
{
	lapic_addr = lapic_eoi_addr = 0;
	ioapic_enabled = 0;

	/*
	 * Keep the BSP's logical CPU ID: cpuid is already stored in the active
	 * bootstrap stack, and the online CPU mask may therefore be sparse even
	 * when SMP initialization falls back to one processor.
	 */
	ncpus = bsp_cpu_id + 1;
}

static void tss_init_all(void)
{
	unsigned cpu;

	for(cpu = 0; cpu < ncpus ; cpu++)
		tss_init(cpu, get_k_stack_top(cpu)); 
}

static int discover_cpus(void)
{
	struct acpi_madt_lapic * cpu;

	while (ncpus < CONFIG_MAX_CPUS && (cpu = acpi_get_lapic_next())) {
		apicid2cpuid[cpu->apic_id] = ncpus;
		cpuid2apicid[ncpus] = cpu->apic_id;
		printf("CPU %3d local APIC id %3d\n", ncpus, cpu->apic_id);
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
	setup_sysenter_syscall();

	/*
	 * we still run on the boot stack and we cannot use cpuid as its value
	 * wasn't set yet. apicid2cpuid initialized in mps_init()
	 */
	bsp_cpu_id = apicid2cpuid[apicid()];

	/*
	 * tss_init_all() seeded the cpuid slot (top-of-stack[-1]) only on the
	 * real per-CPU stacks (get_k_stack_top()); the BSP is still on the
	 * separate boot stack (k_boot_stk), whose slot is uninitialized.
	 * lapic_enable() below calibrates the LAPIC timer with interrupts
	 * enabled, and the clock IRQ reads cpuid in context_stop() to index
	 * kernel_ticks[] &c.  Seed the boot stack's slot with the BSP id now so
	 * that read is valid until we switch_k_stack().  The cpuid macro is an
	 * lvalue using the same %rbp-rounded computation, so this writes exactly
	 * the slot the IRQ will read while we run on the boot stack.
	 */
	cpuid = bsp_cpu_id;

	if (!lapic_enable(bsp_cpu_id)) {
		printf("ERROR : failed to initialize BSP Local APIC\n");
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

	BOOT_VERBOSE(printf("SMP initialized\n"));

	switch_k_stack(X86_STACK_SWITCH_SP(bsp_cpu_id),
			smp_start_aps);
	
	return;

uniproc_fallback:
	apic_idt_init(1); /* Reset to PIC idt ! */
	idt_reload();
	smp_reinit_vars (); /* revert to a single proc system. */
	intr_init(0); /* no auto eoi */
	printf("WARNING : SMP initialization failed\n");
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
