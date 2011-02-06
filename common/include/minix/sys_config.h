#ifndef _MINIX_SYS_CONFIG_H
#define _MINIX_SYS_CONFIG_H 1

/*===========================================================================*
 *		This section contains user-settable parameters		     *
 *===========================================================================*/
#define _MINIX_MACHINE       _MACHINE_IBM_PC

#define _MACHINE_IBM_PC             1	/* any  8088 or 80x86-based system */

/* Word size in bytes (a constant equal to sizeof(int)). */
#if __ACK__ || __GNUC__
#define _WORD_SIZE	_EM_WSIZE
#define _PTR_SIZE	_EM_WSIZE
#endif

#define _NR_PROCS	256
#define _NR_SYS_PROCS	64

/* Set the CHIP type based on the machine selected. The symbol CHIP is actually
 * indicative of more than just the CPU.  For example, machines for which
 * CHIP == INTEL are expected to have 8259A interrrupt controllers and the
 * other properties of IBM PC/XT/AT/386 types machines in general. */
#define _CHIP_INTEL             1	/* CHIP type for PC, XT, AT, 386 and clones */
#define _CHIP_M68000            2	/* CHIP type for Atari, Amiga, Macintosh    */
#define _CHIP_SPARC             3	/* CHIP type for SUN-4 (e.g. SPARCstation)  */

/* Set the FP_FORMAT type based on the machine selected, either hw or sw    */
#define _FP_NONE		  0	/* no floating point support                */
#define _FP_IEEE		  1	/* conform IEEE floating point standard     */

#if (_MINIX_MACHINE == _MACHINE_IBM_PC)
#define _MINIX_CHIP          _CHIP_INTEL
#endif

#ifndef _MINIX_FP_FORMAT
#define _MINIX_FP_FORMAT   _FP_NONE
#endif

#ifndef _MINIX_MACHINE
error "In <minix/sys_config.h> please define _MINIX_MACHINE"
#endif

#ifndef _MINIX_CHIP
error "In <minix/sys_config.h> please define _MINIX_MACHINE to have a legal value"
#endif

#if (_MINIX_MACHINE == 0)
error "_MINIX_MACHINE has incorrect value (0)"
#endif

/* Kernel debug checks */
#define DEBUG_LOCK_CHECK 1	/* Interrupt Lock/unlock sanity checking. */

#define _KMESS_BUF_SIZE  10000

#endif /* _MINIX_SYS_CONFIG_H */
