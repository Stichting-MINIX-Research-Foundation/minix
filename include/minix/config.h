#ifndef _CONFIG_H
#define _CONFIG_H

/* Minix release and version numbers. */
#define OS_RELEASE "3"
#define OS_VERSION "1.6"

/* This file sets configuration parameters for the MINIX kernel, FS, and PM.
 * It is divided up into two main sections.  The first section contains
 * user-settable parameters.  In the second section, various internal system
 * parameters are set based on the user-settable parameters.
 *
 * Parts of config.h have been moved to sys_config.h, which can be included
 * by other include files that wish to get at the configuration data, but
 * don't want to pollute the users namespace. Some editable values have
 * gone there.
 *
 */

/* The MACHINE (called _MINIX_MACHINE) setting can be done
 * in <minix/machine.h>.
 */
#include <minix/sys_config.h>

#define MACHINE      _MINIX_MACHINE

#define IBM_PC       _MACHINE_IBM_PC
#define SUN_4        _MACHINE_SUN_4
#define SUN_4_60     _MACHINE_SUN_4_60
#define ATARI        _MACHINE_ATARI
#define MACINTOSH    _MACHINE_MACINTOSH

/* Number of slots in the process table for non-kernel processes. The number
 * of system processes defines how many processes with special privileges 
 * there can be. User processes share the same properties and count for one. 
 *
 * These can be changed in sys_config.h.
 */
#define NR_PROCS 	  _NR_PROCS 
#define NR_SYS_PROCS      _NR_SYS_PROCS
#define NR_SYS_CHUNKS	  BITMAP_CHUNKS(NR_SYS_PROCS)

/* Number of controller tasks (/dev/cN device classes). */
#define NR_CTRLRS          2

/* DMA_SECTORS may be increased to speed up DMA based drivers. */
#define DMA_SECTORS        1	/* DMA buffer size (must be >= 1) */

/* Which processes should receive diagnostics from the kernel and system? 
 * Directly sending it to TTY only displays the output. Sending it to the
 * log driver will cause the diagnostics to be buffered and displayed.
 * Messages are sent by src/lib/sysutil/kputc.c to these processes, in
 * the order of this array, which must be terminated by NONE. This is used
 * by drivers and servers that printf().
 * The kernel does this for its own kprintf() in kernel/utility.c, also using
 * this array, but a slightly different mechanism.
 */
#define OUTPUT_PROCS_ARRAY	{ TTY_PROC_NR, LOG_PROC_NR, NONE }

/* NR_CONS, NR_RS_LINES, and NR_PTYS determine the number of terminals the
 * system can handle.
 */
#define NR_CONS            4	/* # system consoles (1 to 8) */
#define	NR_RS_LINES	   4	/* # rs232 terminals (0 to 4) */
#define	NR_PTYS		   32	/* # pseudo terminals (0 to 64) */

/* This feature enable the counting of system calls in PM and FS */
#define ENABLE_SYSCALL_STATS	0

/*===========================================================================*
 *	There are no user-settable parameters after this line		     *
 *===========================================================================*/
/* Set the CHIP type based on the machine selected. The symbol CHIP is actually
 * indicative of more than just the CPU.  For example, machines for which
 * CHIP == INTEL are expected to have 8259A interrrupt controllers and the
 * other properties of IBM PC/XT/AT/386 types machines in general. */
#define INTEL             _CHIP_INTEL	/* CHIP type for PC, XT, AT, 386 and clones */
#define M68000            _CHIP_M68000	/* CHIP type for Atari, Amiga, Macintosh    */
#define SPARC             _CHIP_SPARC	/* CHIP type for SUN-4 (e.g. SPARCstation)  */

/* Set the FP_FORMAT type based on the machine selected, either hw or sw    */
#define FP_NONE	 _FP_NONE	/* no floating point support                */
#define FP_IEEE	 _FP_IEEE	/* conform IEEE floating point standard     */

/* _MINIX_CHIP is defined in sys_config.h. */
#define CHIP	_MINIX_CHIP

/* _MINIX_FP_FORMAT is defined in sys_config.h. */
#define FP_FORMAT	_MINIX_FP_FORMAT

/* _ASKDEV and _FASTLOAD are defined in sys_config.h. */
#define ASKDEV _ASKDEV
#define FASTLOAD _FASTLOAD

/* Enable or disable system profiling. */
#define SPROFILE          1    /* statistical profiling */
#define CPROFILE          0    /* call profiling */

/* PCI configuration parameters */
#define NR_PCIBUS 40
#define NR_PCIDEV 50

#endif /* _CONFIG_H */
