#ifndef _IBM_CPU_H
#define _IBM_CPU_H 1

#define X86_FLAG_C	(1L << 0)	/* S  Carry */
#define X86_FLAG_P	(1L << 2)	/* S  Parity */
#define X86_FLAG_A	(1L << 4)	/* S  Aux. carry */
#define X86_FLAG_Z	(1L << 6)	/* S  Zero */
#define X86_FLAG_S	(1L << 7)	/* S  Sign */

#define X86_FLAG_T	(1L <<  8)	/* X  Trap */
#define X86_FLAG_I	(1L <<  9)	/* X  Interrupt */
#define X86_FLAG_D	(1L << 10)	/* C  Direction */
#define X86_FLAG_O	(1L << 11)	/* S  Overflow */

/* User flags are S (Status) and C (Control) flags. */
#define X86_FLAGS_USER (X86_FLAG_C | X86_FLAG_P | X86_FLAG_A | X86_FLAG_Z | \
	X86_FLAG_S | X86_FLAG_D | X86_FLAG_O)

#include <x86/cpu.h>

#endif
