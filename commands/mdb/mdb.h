/*
 * mdb.h for mdb
 */ 
#define MDBVERSION	"2.6"
#define MDBBUILD	0

#define	MINIX_SYMBOLS	1
#define	GNU_SYMBOLS	2

/*
 * Handle options here 
 */
#ifndef GNU_SUPPORT
#define	GNU_SUPPORT		0
#endif

#ifndef SYSCALLS_SUPPORT
#define SYSCALLS_SUPPORT	0
#endif

#ifdef DEBUG
#undef DEBUG
#endif

#ifdef  NDEBUG
#undef  DEBUG
#else
#define DEBUG			1
#endif

#ifdef	__i386
#define EXTRA_SYMBOLS		GNU_SUPPORT
#else
#define EXTRA_SYMBOLS		0
#endif


#include <minix/config.h>
#include <ansi.h>
#include <sys/types.h>

#include <minix/const.h>
#include <minix/type.h>

#include <limits.h>
#include <errno.h>
#include <minix/ipc.h>
#include <timers.h>

#undef printf		/* defined as printk in <minix/const.h> */

#if (CHIP == M68000)
#define __mc68000__	/* controls processor-dependent stuff */
#error "only the MINIX_ST 1.5.x implementation works on 68K's"
#endif

#if (CHIP == INTEL)
#if (MACHINE == IBM_PC)
#define MINIX_PC
#else
#error "only the MINIX_PC 1.5.x and later versions works on *86's"
#endif
#endif

#ifdef MINIX_ST	
#define BP_OFF ((long)&((struct proc *)0)->p_reg.a6)
#define PC_MEMBER(rp) ((rp)->p_reg.pc)
#define PC_OFF ((long)&((struct proc *)0)->p_reg.pc)
#define SP_MEMBER(rp) ((rp)->p_reg.sp)
#define PSW_MEMBER(rp) ((rp)->p_reg.psw)
#endif

#ifdef MINIX_PC
#define BP_OFF ((long)&((struct proc *)0)->p_reg.fp)
#define PC_MEMBER(rp) ((rp)->p_reg.pc)
#define PC_OFF ((long)&((struct proc *)0)->p_reg.pc)
#endif

#define ADDRSIZE	_WORD_SIZE
#define BITSIZE(size)	(8 * (size))
#define INTSIZE		(sizeof(int))	/* not quite right for cross-debugger */
#define LONGSIZE	(sizeof(long))
#define UCHAR(x)	((x) & 0xFF)
#define NOSEG		(-1)	/* no segment */

/* use hardware codes for segments for simplest decoding */
#define CSEG		0x2E	/* 8088 through 80386 */
#define DSEG		0x3E

#if (CHIP == INTEL )
#ifdef __i86
#define N_REG16	2
#endif
#ifdef __i386
#define N_REG16	4  /* 16 bit registers at start of stackframe */ 
#endif
#ifndef N_REG16
#error "N_REG16 not defined"
#endif
#endif

#if (CHIP == INTEL )
#define ADDA(l) ((u16_t) (l) == 0xC481)

#ifdef __i386
#define ADDA_CNT(l) ((i32_t) (l))
#else
#define ADDA_CNT(l) ((i16_t) (l))
#endif

#define ADDQ(l) ((u16_t) (l) == 0xC483)
#define ADDQ_CNT(l) (((((l) >> 16) + 128) & 0x000000FF) - 128)
#define BREAK(l) (0x000000CC | ((l) & 0xFFFFFF00))
#define BREAKPOINT_ADVANCE 1
#define INCSP2(l) ((u16_t) (l) == 0x4444)
#define POPBX2(l) ((u16_t) (l) == 0x5B5B)
#define POPBX(l)  ( (l & 0xFF) == 0x5B) 

/* Added for ANSI CC */
#define POPCX2(l) ((u16_t) (l) == 0x5959)
#define POPCX(l)  ( (l & 0xFF) == 0x59) 

#endif

#ifdef __mc68000__
#define ADDA(l) ((int)((l) >> 16) == 0xDFFC)
#define ADDA_CNT(l) (l)
#define ADDQ(l) (((l >> 16) & 0xF13F) == 0x500F)
#define ADDQ_CNT(l) (((((l) >> 25) - 1) & 7) + 1)
#define BREAK(l) (0xA0000000 | ((l) & 0xFFFF))
#define BREAKPOINT_ADVANCE 0
#define BYTES_SWAPPED	/* this assumes WORDS_SWAPPED too */
#define LEA(l) (((l) >> 16) == 0x4FEF)
#define LEA_DISP(l) ((long)( l & 0xFFFF)) 
#endif

#define MASK(size) ((size) >= LONGSIZE ? -1L : (1L << BITSIZE(size)) - 1)

#ifdef BYTES_SWAPPED
#define SHIFT(size) BITSIZE(LONGSIZE - (size))
#else
#define SHIFT(size) (0)
#endif

#ifdef _MAIN_MDB
#undef EXTERN
#define EXTERN
#endif

extern long lbuf[];		/* buffer for proc	  */ 

EXTERN long st_addr;		/* starting address of text  */
EXTERN long et_addr;		/* ending address of text    */
EXTERN long sd_addr;		/* starting address of data  */
EXTERN long ed_addr;		/* ending address of data  */
EXTERN long end_addr;		/* ending address of text/data */
EXTERN long sk_addr;		/* starting address of stack   */
EXTERN long sk_size;		/* size of stack   */
EXTERN int curpid;		/* current pid of process/core */
EXTERN int corepid;		/* pid of core file */
EXTERN int coreonly;		/* core file only   */
EXTERN int fileonly;		/* file only        */
EXTERN int cursig;		/* current signal   */
EXTERN int seg;			/* segment 	    */
EXTERN int is_separate;		/* separate I & D   */ 
EXTERN int paging;		/* paging flag      */
#ifdef	DEBUG
EXTERN int debug;		/* debug flag	    */
#endif
#if	SYSCALLS_SUPPORT
EXTERN int syscalls;		/* trace syscalls   */
#endif

#ifdef _MAIN_MDB
#undef EXTERN
#define EXTERN extern
#endif

