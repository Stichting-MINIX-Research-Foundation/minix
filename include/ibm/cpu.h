#ifndef _IBM_CPU_H
#define _IBM_CPU_H 1

#define X86_FLAG_C	(1L << 0)	/* Carry */
#define X86_FLAG_P	(1L << 2)	/* Parity */
#define X86_FLAG_A	(1L << 4)	/* Aux. carry */
#define X86_FLAG_Z	(1L << 6)	/* Zero */
#define X86_FLAG_S	(1L << 7)	/* Sign */

#define X86_FLAG_T	(1L <<  8)	/* Trap */
#define X86_FLAG_I	(1L <<  9)	/* Interrupt */
#define X86_FLAG_D	(1L << 10)	/* Direction */
#define X86_FLAG_O	(1L << 11)	/* Overflow */

#endif
