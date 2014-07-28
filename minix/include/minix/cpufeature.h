
#ifndef _MINIX_CPUFEATURE_H
#define _MINIX_CPUFEATURE_H 1

#define _CPUF_I386_FPU		0	/* FPU-x87 FPU on Chip */
#define _CPUF_I386_PSE 		1	/* Page Size Extension */
#define _CPUF_I386_PGE 		2	/* Page Global Enable */
#define _CPUF_I386_APIC_ON_CHIP	3	/* APIC is present on the chip */
#define _CPUF_I386_TSC		4	/* Timestamp counter present */
#define _CPUF_I386_SSE1234_12	5	/* Support for SSE/SSE2/SSE3/SSSE3/SSE4
					 * Extensions and FXSR
					 */
#define _CPUF_I386_FXSR		6
#define _CPUF_I386_SSE		7
#define _CPUF_I386_SSE2		8
#define _CPUF_I386_SSE3		9
#define _CPUF_I386_SSSE3	10
#define _CPUF_I386_SSE4_1	11
#define _CPUF_I386_SSE4_2	12

#define _CPUF_I386_HTT		13	/* Supports HTT */
#define _CPUF_I386_HTT_MAX_NUM	14	/* Maximal num of threads */

#define _CPUF_I386_MTRR		15
#define _CPUF_I386_SYSENTER	16	/* Intel SYSENTER instrs */
#define _CPUF_I386_SYSCALL	17	/* AMD SYSCALL instrs */

int _cpufeature(int featureno);

#endif
