/* read_tsc / read_host_time_ns — x86_64 */
#include <machine/asm.h>

/* void read_tsc(unsigned long *high, unsigned long *low); */
/* Read the cycle counter.  RDTSC puts result in EDX:EAX. */
ENTRY(read_tsc)
	rdtsc				/* EDX:EAX = TSC */
	movl	%edx, (%rdi)		/* *high = edx */
	movl	%eax, (%rsi)		/* *low  = eax */
	ret

/* void read_host_time_ns(unsigned long *high, unsigned long *low); */
/* Access real-time in ns from host in VMware (ECX=0x10001 selects it). */
ENTRY(read_host_time_ns)
	movl	$0x10001, %ecx
	rdtsc				/* EDX:EAX = host time ns */
	movl	%edx, (%rdi)		/* *high = edx */
	movl	%eax, (%rsi)		/* *low  = eax */
	ret
