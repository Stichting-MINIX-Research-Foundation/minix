#ifndef __SMP_X86_H__
#define __SMP_X86_H__

#include "arch_proto.h" /* K_STACK_SIZE */

// Added kernel headers (precautionary for consistency)
#include <minix/kernel_types.h> // For fixed-width types like k_uint32_t
#include <klib/include/kprintf.h>
#include <klib/include/kstring.h>
#include <klib/include/kmemory.h>


#define MAX_NR_INTERRUPT_ENTRIES	128

#ifndef __ASSEMBLY__

/* returns the current cpu id */
// FIXME: u32_t might be undefined
#define cpuid	(((u32_t *)(((u32_t)get_stack_frame() + (K_STACK_SIZE - 1)) \
						& ~(K_STACK_SIZE - 1)))[-1])
/* 
 * in case apic or smp is disabled in boot monitor, we need to finish single cpu
 * boot using the legacy PIC
 */
#define smp_single_cpu_fallback() do {		\
	  tss_init(0, get_k_stack_top(0));	\
	  bsp_cpu_id = 0;			\
	  ncpus = 1;				\
	  bsp_finish_booting();			\
} while(0)

extern unsigned char cpuid2apicid[CONFIG_MAX_CPUS];

#define barrier()	do { mfence(); } while(0)

#endif

#endif /* __SMP_X86_H__ */
