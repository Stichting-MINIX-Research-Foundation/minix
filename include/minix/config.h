#ifndef _CONFIG_H
#define _CONFIG_H

/* Minix release and version numbers. */
#define OS_RELEASE "3"
#define OS_VERSION "0.6"

#define _MINIX_VERSION	3

/* This file sets configuration parameters for the MINIX kernel, FS, and PM.
 * It is divided up into two main sections.  The first section contains
 * user-settable parameters.  In the second section, various internal system
 * parameters are set based on the user-settable parameters.
 */

/*===========================================================================*
 *		This section contains user-settable parameters		     *
 *===========================================================================*/
#define MACHINE       IBM_PC	/* Must be one of the names listed below */

#define IBM_PC             1	/* any  8088 or 80x86-based system */
#define SUN_4             40	/* any Sun SPARC-based system */
#define SUN_4_60	  40	/* Sun-4/60 (aka SparcStation 1 or Campus) */
#define ATARI             60	/* ATARI ST/STe/TT (68000/68030) */
#define MACINTOSH         62	/* Apple Macintosh (68000) */

/* Word size in bytes (a constant equal to sizeof(int)). */
#if __ACK__
#define _WORD_SIZE	_EM_WSIZE
#define _PTR_SIZE	_EM_WSIZE
#endif

/* Number of slots in the process table for non-kernel processes. The number
 * of system processes defines how many processes with special privileges 
 * there can be. User processes share the same properties and count for one. 
 */
#define NR_PROCS 	  64 
#define NR_SYS_PROCS      32

/* The buffer cache should be made as large as you can afford. */
#if (MACHINE == IBM_PC && _WORD_SIZE == 2)
#define NR_BUFS           40	/* # blocks in the buffer cache */
#define NR_BUF_HASH       64	/* size of buf hash table; MUST BE POWER OF 2*/
#endif

#if (MACHINE == IBM_PC && _WORD_SIZE == 4)
#define NR_BUFS         1536	/* # blocks in the buffer cache */
#define NR_BUF_HASH     2048	/* size of buf hash table; MUST BE POWER OF 2*/
#endif

#if (MACHINE == SUN_4_60)
#define NR_BUFS		 512	/* # blocks in the buffer cache (<=1536) */
#define NR_BUF_HASH	 512	/* size of buf hash table; MUST BE POWER OF 2*/
#endif

/* Defines for driver and kernel configuration. */
#define AUTO_BIOS          0	/* xt_wini.c - use Western's autoconfig BIOS */
#define LINEWRAP           1	/* console.c - wrap lines at column 80 */

/* Number of controller tasks (/dev/cN device classes). */
#define NR_CTRLRS          2

/* Enable or disable the second level file system cache on the RAM disk. */
#define ENABLE_CACHE2      0

/* Enable or disable swapping processes to disk. */
#define ENABLE_SWAP	   1

/* Include or exclude an image of /dev/boot in the boot image. */
#define ENABLE_BOOTDEV	   0

/* Include or exclude device drivers.  Set to 1 to include, 0 to exclude. */
#define ENABLE_BIOS_WINI   1	/* enable BIOS winchester driver */
#define ENABLE_ESDI_WINI   0	/* enable ESDI winchester driver */
#define ENABLE_XT_WINI     0	/* enable XT winchester driver */
#define ENABLE_AHA1540     0	/* enable Adaptec 1540 SCSI driver */
#define ENABLE_FATFILE     0	/* enable FAT file virtual disk driver */
#define ENABLE_DOSFILE     0	/* enable DOS file virtual disk driver */
#define ENABLE_SB16        0	/* enable Soundblaster audio driver */
#define ENABLE_PCI	   1	/* enable PCI device recognition */

/* Include or exclude user-level device drivers (and supporting servers). */
#define ENABLE_PRINTER     0	/* user-level Centronics printer driver */
#define ENABLE_FLOPPY      0	/* enable floppy disk driver */
#define ENABLE_AT_WINI     1	/* enable AT winchester driver */
#define   ENABLE_ATAPI     1	/* add ATAPI support to AT driver */

/* DMA_SECTORS may be increased to speed up DMA based drivers. */
#define DMA_SECTORS        1	/* DMA buffer size (must be >= 1) */

/* Include or exclude backwards compatibility code. */
#define ENABLE_BINCOMPAT   0	/* for binaries using obsolete calls */
#define ENABLE_SRCCOMPAT   0	/* for sources using obsolete calls */

/* Which process should receive diagnostics from the kernel and system? 
 * Directly sending it to TTY only displays the output. Sending it to the
 * log driver will cause the diagnostics to be buffered and displayed.
 */
#define PRINTF_PROC		LOG_PROC_NR	/* TTY or LOG_PROC_NR */

/* NR_CONS, NR_RS_LINES, and NR_PTYS determine the number of terminals the
 * system can handle.
 */
#define NR_CONS            4	/* # system consoles (1 to 8) */
#define	NR_RS_LINES	   4	/* # rs232 terminals (0 to 4) */
#define	NR_PTYS		   32	/* # pseudo terminals (0 to 64) */


/*===========================================================================*
 *	There are no user-settable parameters after this line		     *
 *===========================================================================*/
/* Set the CHIP type based on the machine selected. The symbol CHIP is actually
 * indicative of more than just the CPU.  For example, machines for which
 * CHIP == INTEL are expected to have 8259A interrrupt controllers and the
 * other properties of IBM PC/XT/AT/386 types machines in general. */
#define INTEL             1	/* CHIP type for PC, XT, AT, 386 and clones */
#define M68000            2	/* CHIP type for Atari, Amiga, Macintosh    */
#define SPARC             3	/* CHIP type for SUN-4 (e.g. SPARCstation)  */

/* Set the FP_FORMAT type based on the machine selected, either hw or sw    */
#define FP_NONE		  0	/* no floating point support                */
#define FP_IEEE		  1	/* conform IEEE floating point standard     */

#if (MACHINE == IBM_PC)
#define CHIP          INTEL
#endif

#if (MACHINE == ATARI) || (MACHINE == MACINTOSH)
#define CHIP         M68000
#endif

#if (MACHINE == SUN_4) || (MACHINE == SUN_4_60)
#define CHIP          SPARC
#define FP_FORMAT   FP_IEEE
#endif

#if (MACHINE == ATARI) || (MACHINE == SUN_4)
#define ASKDEV            1	/* ask for boot device */
#define FASTLOAD          1	/* use multiple block transfers to init ram */
#endif

#if (ATARI_TYPE == TT) /* and all other 68030's */
#define FPP
#endif

#ifndef FP_FORMAT
#define FP_FORMAT   FP_NONE
#endif

#ifndef MACHINE
error "In <minix/config.h> please define MACHINE"
#endif

#ifndef CHIP
error "In <minix/config.h> please define MACHINE to have a legal value"
#endif

#if (MACHINE == 0)
error "MACHINE has incorrect value (0)"
#endif

#endif /* _CONFIG_H */
