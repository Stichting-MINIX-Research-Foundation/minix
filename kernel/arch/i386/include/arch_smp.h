#ifndef __SMP_X86_H__
#define __SMP_X86_H__

#include "arch_proto.h" /* K_STACK_SIZE */

#define MAX_NR_INTERRUPT_ENTRIES	128

#define SMP_SCHED_PROC			0xF0
#define SMP_DEQUEUE_PROC		0xF1
#define SMP_CPU_REBOOT			0xF2
#define SMP_CPU_HALT			0xF3
#define SMP_ERROR_INT			0xF4

/* currently only 2 interrupt priority levels are used */
#define SPL0				0x0
#define	SPLHI				0xF

#define SMP_IPI_DEST			0
#define SMP_IPI_SELF			1
#define SMP_IPI_TO_ALL			2
#define SMP_IPI_TO_ALL_BUT_SELF		3

#ifndef __ASSEMBLY__

/* returns the current cpu id */
#define cpuid	(((u32_t *)(((u32_t)get_stack_frame() + (K_STACK_SIZE - 1)) \
						& ~(K_STACK_SIZE - 1)))[-1])
/* 
 * in case apic or smp is disabled in boot monitor, we need to finish single cpu
 * boot using the legacy PIC
 */
#define smp_single_cpu_fallback() do {		\
	  tss_init(0, get_k_stack_top(0));	\
	  bsp_finish_booting();			\
} while(0)

extern unsigned char cpuid2apicid[CONFIG_MAX_CPUS];

#endif

#endif /* __SMP_X86_H__ */

