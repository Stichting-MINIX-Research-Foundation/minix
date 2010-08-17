/*  */
/* sections */
#include <machine/asm.h>

/**===========================================================================* */
/* PUBLIC void read_tsc(unsigned long *high, unsigned long *low); */
/* Read the cycle counter of the CPU. Pentium and up.  */
ENTRY(read_tsc)
	push	%edx
	push	%eax
.byte	0x0f	/* this is the RDTSC instruction  */
.byte	0x31	/* it places the TSC in EDX:EAX */
	push	%ebp
	movl	16(%esp), %ebp
	movl	%edx, (%ebp)
	movl	20(%esp), %ebp
	movl	%eax, (%ebp)
	pop	%ebp
	pop	%eax
	pop	%edx
	ret

/**===========================================================================* */
/* PUBLIC void read_host_time_ns(unsigned long *high, unsigned long *low); */
/* access real time in ns from host in vmware. */
ENTRY(read_host_time_ns)
        pushl   %edx
        pushl   %eax
        pushl   %ecx
        movl    $0x10001, %ecx
.byte	0x0f	/* this is the RDTSC instruction  */
.byte	0x31	/* it places the TSC in EDX:EAX */
        pushl   %ebp
        movl    20(%esp), %ebp
        movl    %edx, (%ebp)
        movl    24(%esp), %ebp
        movl    %eax, (%ebp)
        popl    %ebp
        popl    %ecx
        popl    %eax
        popl    %edx
        ret
