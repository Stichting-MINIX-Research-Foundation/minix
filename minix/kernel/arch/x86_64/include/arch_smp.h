#ifndef __SMP_X86_H__
#define __SMP_X86_H__

#include "arch_proto.h" /* K_STACK_SIZE */

#define MAX_NR_INTERRUPT_ENTRIES	128

#ifndef __ASSEMBLY__

/*
 * Returns the current cpu id.  The id is stored as a reg_t at
 * kernel_stack_top - sizeof(reg_t) by tss_init() (see protect.c).  Use
 * vir_bytes (64-bit) for the address arithmetic so the kernel stack VA at
 * 0xFFFFFFFF80xxxxxx is not truncated, and read it as a reg_t so [-1] lands
 * on the full 8-byte slot rather than its high (zero) half.
 */
#define cpuid	(((reg_t *)(((vir_bytes)get_stack_frame() + (K_STACK_SIZE - 1)) \
						& ~(K_STACK_SIZE - 1)))[-1])

/*
 * switch_k_stack() jumps to its continuation; it does not push a return
 * address as CALL would.  Leave one word below the two per-CPU words
 * reserved by tss_init(), so a C continuation starts with the SysV AMD64
 * ABI stack alignment (RSP % 16 == 8).
 */
#define X86_STACK_SWITCH_SP(cpu)	\
	((char *)get_k_stack_top(cpu) - (3 * sizeof(reg_t)))
/*
 * If APIC or SMP is disabled in the boot monitor, finish booting on the
 * legacy PIC.  As in the normal SMP path, move off the EFI bootstrap stack
 * before entering bsp_finish_booting(): timer initialization enables
 * interrupts and the interrupt path needs the dedicated per-CPU stack (and
 * its CPU-id slot, initialized by tss_init()).
 */
#define smp_single_cpu_fallback() do {		\
	  tss_init(0, get_k_stack_top(0));	\
	  setup_sysenter_syscall();		\
	  bsp_cpu_id = 0;			\
	  ncpus = 1;				\
	  switch_k_stack(X86_STACK_SWITCH_SP(0), \
			  bsp_finish_booting);		\
} while(0)

extern unsigned char cpuid2apicid[CONFIG_MAX_CPUS];

#define barrier()	do { mfence(); } while(0)

#endif

#endif /* __SMP_X86_H__ */
